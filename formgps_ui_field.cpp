// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// GUI to backend field interface
#include "formgps.h"
#include "qmlutil.h"
#include "newsettings.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

#include "cboundarylist.h"

double latK, lonK = 0.0;

void FormGPS::field_update_list() {

#ifdef __ANDROID__
    QString directoryName = androidDirectory + QCoreApplication::applicationName() + "/Fields";
#else
    QString directoryName = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                            + "/" + QCoreApplication::applicationName() + "/Fields";
#endif

    QObject *fieldInterface = qmlItem(mainWindow, "fieldInterface");
    QList<QVariant> fieldList;
    QMap<QString, QVariant> field;
    int index = 0;

    QDirIterator it(directoryName, QStringList() << "Field.txt", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()){
        field = FileFieldInfo(it.next());
        if(field.contains("latitude")) {
            field["index"] = index;
            fieldList.append(field);
            index++;
        }
    }
    fieldInterface->setProperty("field_list", fieldList);
}

void FormGPS::field_close() {
    qDebug() << "field_close";
    FileSaveEverythingBeforeClosingField();
}

void FormGPS::field_open(QString field_name) {
    qDebug() << "field_open";
    FileSaveEverythingBeforeClosingField();
    if (! FileOpenField(field_name)) {
        TimedMessageBox(8000, tr("Saved field does not exist."), QString(tr("Cannot find the requested saved field.")) + " " +
                                                                field_name);

        settings->setValue(SETTINGS_f_currentDir, "Default");
    }
}

void FormGPS::field_new(QString field_name) {
    //assume the GUI will vet the name a little bit
    lock.lockForWrite();
    FileSaveEverythingBeforeClosingField();
    currentFieldDirectory = field_name.trimmed();
    settings->setValue(SETTINGS_f_currentDir, currentFieldDirectory);
    JobNew();

    pn.latStart = pn.latitude;
    pn.lonStart = pn.longitude;
    pn.SetLocalMetersPerDegree();

    FileCreateField();
    FileCreateSections();
    FileCreateRecPath();
    FileCreateContour();
    FileCreateElevation();
    FileSaveFlags();
    FileCreateBoundary();
    FileSaveTram();
    lock.unlock();
}

void FormGPS::field_new_from(QString existing, QString field_name, int flags) {
    qDebug() << "field_new_from";
    lock.lockForWrite();
    FileSaveEverythingBeforeClosingField();
    if (! FileOpenField(existing,flags)) { //load whatever is requested from existing field
        TimedMessageBox(8000, tr("Existing field cannot be found"), QString(tr("Cannot find the existing saved field.")) + " " +
                                                                existing);
    }
    //change to new name
    currentFieldDirectory = field_name;
    settings->setValue(SETTINGS_f_currentDir, currentFieldDirectory);

    FileCreateField();
    FileCreateSections();
    FileCreateElevation();
    FileSaveFlags();
    FileSaveABLines();
    FileSaveCurveLines();

    contourSaveList.clear();
    contourSaveList.append(ct.ptList);
    FileSaveContour();

    FileSaveRecPath();
    FileSaveTram();

    //some how we have to write the existing patches to the disk.
    //FileSaveSections only write pending triangles

    for(QSharedPointer<PatchTriangleList> &l: triStrip[0].patchList) {
        tool.patchSaveList.append(l);
    }
    FileSaveSections();
    lock.unlock();
}

bool parseDouble(const std::string& input, double& output) {
    std::string cleaned = input;

    // Replace comma with dot for decimal point compatibility
    std::replace(cleaned.begin(), cleaned.end(), ',', '.');

    std::istringstream iss(cleaned);
    iss.imbue(std::locale::classic()); // Ensures '.' is the decimal point

    iss >> output;

    // Check for parsing success and no extra characters
    return !iss.fail() && iss.eof();
}

void FindLatLon(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file.\n";
        return;
    }

    std::string line;
    std::string coordinates;
    try {
        while (std::getline(file, line)) {
            size_t startIndex = line.find("<coordinates>");
            if (startIndex != std::string::npos) {
                // Found opening tag
                while (true) {
                    size_t endIndex = line.find("</coordinates>");

                    if (endIndex == std::string::npos) {
                        if (startIndex == std::string::npos)
                            coordinates += line;
                        else
                            coordinates += line.substr(startIndex + 13);  // Skip "<coordinates>"
                    } else {
                        if (startIndex == std::string::npos)
                            coordinates += line.substr(0, endIndex);
                        else
                            coordinates += line.substr(startIndex + 13, endIndex - (startIndex + 13));
                        break;
                    }

                    if (!std::getline(file, line)) break;
                    line.erase(0, line.find_first_not_of(" \t\n\r\f\v")); // trim
                    startIndex = std::string::npos;
                }

                // Split the coordinates by whitespace
                std::istringstream ss(coordinates);
                std::string item;
                std::vector<std::string> numberSets;
                while (ss >> item) {
                    numberSets.push_back(item);
                }

                if (numberSets.size() > 2) {
                    double counter = 0, lat = 0, lon = 0;
                    latK = lonK = 0;

                    for (const auto& coord : numberSets) {
                        if (coord.length() < 3) continue;
                        size_t comma1 = coord.find(',');
                        size_t comma2 = coord.find(',', comma1 + 1);
                        if (comma1 == std::string::npos || comma2 == std::string::npos) continue;

                        std::string lonStr = coord.substr(0, comma1);
                        std::string latStr = coord.substr(comma1 + 1, comma2 - comma1 - 1);

                        try {
                            double tempLon, tempLat = 0.0;
                            parseDouble(lonStr,tempLon);
                            parseDouble(latStr,tempLat);
                            lon += tempLon;
                            lat += tempLat;
                            counter += 1;
                        } catch (...) {
                            continue;
                        }
                    }

                    if (counter > 0) {
                        lonK = lon / counter;
                        latK = lat / counter;
                    }

                    coordinates.clear();
                } else {
                    std::cerr << "Error reading KML: Too few coordinate points.\n";
                    return;
                }

                break; // Exit after finding and processing first <coordinates> block
            }
        }
    } catch (...) {
        std::cerr << "Exception: Error Finding Lat Lon.\n";
        return;
    }

    // In original C#: mf.bnd.isOkToAddPoints = false;
    // Here: simulate behavior or ignore
}

void FormGPS::LoadKMLBoundary(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file.\n";
        return;
    }

    std::string line;
    std::string coordinates;
    try {
        while (std::getline(file, line)) {
            size_t startIndex = line.find("<coordinates>");
            if (startIndex != std::string::npos) {
                // Found opening tag
                while (true) {
                    size_t endIndex = line.find("</coordinates>");

                    if (endIndex == std::string::npos) {
                        if (startIndex == std::string::npos)
                            coordinates += line;
                        else
                            coordinates += line.substr(startIndex + 13);  // Skip "<coordinates>"
                    } else {
                        if (startIndex == std::string::npos)
                            coordinates += line.substr(0, endIndex);
                        else
                            coordinates += line.substr(startIndex + 13, endIndex - (startIndex + 13));
                        break;
                    }

                    if (!std::getline(file, line)) break;
                    line.erase(0, line.find_first_not_of(" \t\n\r\f\v")); // trim
                    startIndex = std::string::npos;
                }

                // Split the coordinates by whitespace
                std::istringstream ss(coordinates);
                std::string item;
                std::vector<std::string> numberSets;
                while (ss >> item) {
                    numberSets.push_back(item);
                }

                if (numberSets.size() > 2) {
                    double counter = 0, lat = 0, lon = 0;
                    latK = lonK = 0;

                    CBoundaryList New;

                    for (const auto& coord : numberSets) {
                        if (coord.length() < 3) continue;
                        qDebug() << coord;
                        size_t comma1 = coord.find(',');
                        size_t comma2 = coord.find(',', comma1 + 1);
                        if (comma1 == std::string::npos || comma2 == std::string::npos) continue;

                        std::string lonStr = coord.substr(0, comma1);
                        std::string latStr = coord.substr(comma1 + 1, comma2 - comma1 - 1);

                        try {
                            double easting, northing, tmp1, tmp2 = 0.0;
                            parseDouble(lonStr,lonK);
                            parseDouble(latStr,latK);
                            pn.ConvertWGS84ToLocal(latK, lonK, northing, easting);
                            Vec3 temp(easting, northing, 0);
                            New.fenceLine.append(temp);
                        } catch (...) {
                            continue;
                        }
                    }

                    //build the boundary, make sure is clockwise for outer counter clockwise for inner
                    New.CalculateFenceArea(bnd.bndList.count());
                    New.FixFenceLine(bnd.bndList.count());

                    bnd.bndList.append(New);

                    //btnABDraw.Visible = true;

                    coordinates.clear();
                } else {
                    std::cerr << "Error reading KML: Too few coordinate points.\n";
                    return;
                }

                break; // Exit after finding and processing first <coordinates> block
            }
        }
    } catch (...) {
        std::cerr << "Exception: Error Finding Lat Lon.\n";
        return;
    }

    // In original C#: mf.bnd.isOkToAddPoints = false;
    // Here: simulate behavior or ignore
}

void FormGPS::field_new_from_KML(QString field_name, QString file_name) {
    qDebug() << field_name << " " << file_name;

        //assume the GUI will vet the name a little bit
    lock.lockForWrite();
    FileSaveEverythingBeforeClosingField();
    currentFieldDirectory = field_name.trimmed();
    settings->setValue(SETTINGS_f_currentDir, currentFieldDirectory);
    JobNew();
    file_name.remove("file://");
    FindLatLon(file_name.toStdString());

    pn.latStart = latK;
    pn.lonStart = lonK;
    if (timerSim.isActive())
        {
            pn.latitude = pn.latStart;
            pn.longitude = pn.lonStart;

            sim.latitude = pn.latStart;
            settings->setValue(SETTINGS_gps_simLatitude, (double)pn.latStart);
            sim.longitude = pn.lonStart;
            settings->setValue(SETTINGS_gps_simLongitude, (double)pn.lonStart);
        }
    pn.SetLocalMetersPerDegree();


    FileCreateField();
    FileCreateSections();
    FileCreateRecPath();
    FileCreateContour();
    FileCreateElevation();
    FileSaveFlags();
    FileCreateBoundary();
    FileSaveTram();

    LoadKMLBoundary(file_name.toStdString());
    lock.unlock();
}

void FormGPS::field_delete(QString field_name) {
#ifdef __ANDROID__
    QString directoryName = androidDirectory + QCoreApplication::applicationName() + "/Fields/" + field_name;
#else
    QString directoryName = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                            + "/" + QCoreApplication::applicationName() + "/Fields/" + field_name;
#endif

    QDir fieldDir(directoryName);

    if(! fieldDir.exists()) {
        TimedMessageBox(8000,tr("Cannot find saved field"),QString(tr("Cannot find saved field to delete.")) + " " + field_name);
        return;
    }
    if(!QFile::moveToTrash(directoryName)){
        fieldDir.removeRecursively();
    }
    field_update_list();
}
