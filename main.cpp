// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// main
#include "formgps.h"
#include <QApplication>
#include <QCoreApplication>
#include <QLabel>
#include "classes/settingsmanager.h"  // MASSIVE MIGRATION: New SettingsManager
#include "aogrenderer.h"
#include "classes/agioservice.h"      // For auto-registration + C++ usage
#include "classes/ctrack.h"           // For auto-registration + C++ usage
#include "classes/cvehicle.h"         // For auto-registration + C++ usage
#include "classes/pgnparser.h"        // Phase 6.0.21: For ParsedData metatype registration
#include <QProcess>
#include <QSysInfo>
#include <QTranslator> //for translations
#include <QtQml/QQmlEngine>
#include <QtQml/QJSEngine>
#include <QtQml/qqmlregistration.h>
#include <QLoggingCategory>

QLabel *grnPixelsWindow;
QLabel *overlapPixelsWindow;
// MASSIVE MIGRATION: Settings *settings; REMOVED - replaced by SettingsManager singleton

#ifndef TESTING
int main(int argc, char *argv[])
{
    qputenv("QSG_RENDER_LOOP", "threaded");

#ifdef  Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid()) {
            QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
            if (window.isValid()) {
                const int FLAG_KEEP_SCREEN_ON = 128;
                window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
            }
        }
    });
#endif

    // PHASE 6.0.23.1: Disable debug logs to prevent performance issues (40Hz PGN spam)
    // Phase 6.0.24: Allow selective debug logging for AgIOService (change agioservice.debug=false to true)
    QLoggingCategory::setFilterRules(QStringLiteral(
        "*.debug=false\n"
        "agioservice.debug=false\n"  // Change to true to enable AgIOService debug logs
        "*.qtagopengps.debug=true\n"
        "qt.scenegraph.general=true\n"
        "*.warning=true\n"
        "*.critical=true\n"
        "*.fatal=true"
    ));

    qSetMessagePattern("%{time hh:mm:ss.zzz} [%{type}] %{function}:%{line} - %{message}");
    QApplication a(argc, argv);

    QFont f = a.font();
    f.setPointSize(16);
    a.setFont(f);
    QCoreApplication::setOrganizationName("QtAgOpenGPS");
    QCoreApplication::setOrganizationDomain("qtagopengps");
    QCoreApplication::setApplicationName("QtAgOpenGPS");
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,
                       QSettings::UserScope,
                       QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));

    //We're supposed to be compatible with the saved data
    //from this version of AOG:
    QCoreApplication::setApplicationVersion("4.1.0");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    qRegisterMetaTypeStreamOperators<QList<int> >("QList<int>");
    qRegisterMetaTypeStreamOperators<QVector<int> >("QVector<int>");
#endif

    // Phase 6.0.21: Register PGNParser::ParsedData for Qt::QueuedConnection signals
    qRegisterMetaType<PGNParser::ParsedData>("PGNParser::ParsedData");

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    
    // PHASE 6.0.11.4: Manual QML singleton registration (workaround for qmldir limitation)
    qmlRegisterSingletonType<SettingsManager>("AOG", 1, 0, "SettingsManager",
        [](QQmlEngine *engine, QJSEngine *jsEngine) -> QObject* {
            Q_UNUSED(engine)
            Q_UNUSED(jsEngine)
            return SettingsManager::instance();
        });

    qmlRegisterSingletonType<AgIOService>("AOG", 1, 0, "AgIOService",
        [](QQmlEngine *engine, QJSEngine *jsEngine) -> QObject* {
            Q_UNUSED(engine)
            Q_UNUSED(jsEngine)
            return AgIOService::instance();
        });

    qmlRegisterSingletonType<CVehicle>("AOG", 1, 0, "VehicleInterface",
        [](QQmlEngine *engine, QJSEngine *jsEngine) -> QObject* {
            Q_UNUSED(engine)
            Q_UNUSED(jsEngine)
            return CVehicle::instance();
        });

    // AOGRenderer: Component registration (OpenGL renderers must be instantiated in QML)
    qmlRegisterType<AOGRendererInSG>("AOG", 1, 0, "AOGRenderer");
    qmlRegisterType<AOGRendererItem>("AOG", 1, 0, "AOGRendererItem");

    // MASSIVE MIGRATION: settings = new Settings(); REMOVED
    //AOGProperty::init_defaults();
    // Qt6 Pure Architecture: Properties auto-initialize with defaults, no manual sync needed

    // qDebug() << "=== PHASE 6.0.11.4 QML_SINGLETON AUTOMATIC REGISTRATION ===\n"
    //          << "Qt 6.8 QML_SINGLETON pure architecture active\n"
    //          << "CMAKE_AUTOMOC enabled for automatic registration\n"
    //          << "All singletons: QML_ELEMENT + QML_SINGLETON + qt_add_qml_module\n"
    //          << "Status: Phase 6.0.11.4 automatic registration active";
    FormGPS w;
    //w.show();
    
    // MASSIVE MIGRATION: Validate SettingsManager accessible
    qDebug() << "SettingsManager instance:" << SettingsManager::instance();
    qDebug() << "SettingsManager initialization: completed";

    if (SettingsManager::instance()->display_showBack()) {
        grnPixelsWindow = new QLabel("Back Buffer");
        grnPixelsWindow->setFixedWidth(500);
        grnPixelsWindow->setFixedHeight(500);
        grnPixelsWindow->show();
        overlapPixelsWindow = new QLabel("overlap buffer");
        //overlapPixelsWindow->setFixedWidth(1300);
        //overlapPixelsWindow->setFixedHeight(900);
        overlapPixelsWindow->show();
    }

// //auto start AgIO
// #ifndef __ANDROID__
//     QProcess process;
//     if(SettingsManager::instance()->feature_isAgIOOn()) {
//         QObject::connect(&process, &QProcess::errorOccurred, [&](QProcess::ProcessError error) {
//             if (error == QProcess::Crashed) {
//                 qDebug() << "AgIO Crashed! Continuing QtAgOpenGPS like normal";
//             }
//         });

// //start the application
// #ifdef __WIN32
//         process.start("./QtAgIO.exe");

// #else //assume linux
//         process.start("./QtAgIO/QtAgIO");

// #endif

//         // Ensure process starts successfully
//         if (!process.waitForStarted()) {
//             qWarning() << "AgIO failed to start. Continuing QtAgOpenGPS like normal";
//         }
//     }
// #endif




    /*
    CDubinsTurningRadius = 5.25;

    CDubins c;
    Vec3 start(0,0,0);
    Vec3 goal (8,0,0);
    QVector<Vec3> pathlist;
    pathlist = c.GenerateDubins(start, goal);

    foreach(Vec3 goal: pathlist) {
        qDebug() << goal.easting<< ", "<<goal.northing;
    }
    return 0;
    */

    //Test file I/O
    //w.fileSaveCurveLines();
    //w.fileSaveBoundary();
    //w.fileSaveABLines();
    //w.fileSaveContour();
    //w.fileSaveVehicle("/tmp/TestVehicle.txt");
    //w.fileOpenField("49111 1 1 2020.Mar.21 09_58");
    //w.ABLine.isBtnABLineOn = true;
    //w.hd.isOn = true;

    //w.ABLine.isBtnABLineOn = true;
    //w.fileOpenTool("/tmp/TestTool1.txt");
    //w.fileOpenVehicle("/tmp/TestVehicle2.txt");
    //w.fileSaveTool("/tmp/TestTool.TXT");
    /*
    //testing to see how locale affects numbers in the stream writer
    QFileInfo testit("/tmp/noexistant/file.txt");
    qDebug() << testit.baseName();
    qDebug() << testit.suffix();
    QFile testFile("/tmp/test.txt");
    testFile.open(QIODevice::WriteOnly);
    QTextStream writer(&testFile);
    writer << "Testing" << Qt::endl;
    writer << qSetFieldWidth(3) << (double)3.1415926535 << Qt::endl;
    testFile.close();
    */

    return a.exec();
}
#endif
