// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// This runs every time we get a new GPS fix, or sim position
#include "formgps.h"
#include "cnmea.h"
#include "cmodulecomm.h"
#include "ccontour.h"
#include "cvehicle.h"
#include "csection.h"
#include "cboundary.h"
#include "ctrack.h"
#include "settingsmanager.h"
#include <QQuickView>
#include <QOpenGLContext>
#include <QPair>
#include <QElapsedTimer>
#include <QLabel>
#include <QPainter>
#include "glm.h"
#include "aogrenderer.h"
#include "cpgn.h"
#include "qmlutil.h"
#include "glutils.h"
#include <QtConcurrent/QtConcurrentRun>


Q_LOGGING_CATEGORY (qpos, "formgps_position.qtagopengps")

extern QLabel *grnPixelsWindow;
extern QLabel *overlapPixelsWindow;

//called for every new GPS or simulator position
void FormGPS::UpdateFixPosition()
{
    QLocale locale;

    // PHASE 6.0.33: Declare rawGpsPosition at function start (before goto labels)
    // Used to separate RAW GPS positions (for heading calc) from CORRECTED positions (for display)
    // Now copies from m_rawGpsPosition member (set by onNmeaDataReady at 8 Hz)
    Vec2 rawGpsPosition;

    //swFrame.Stop();
    //Measure the frequency of the GPS updates
    //timeSliceOfLastFix = (double)(swFrame.elapsed()) / 1000;
    qDebug(qpos) << "swFrame time at new frame: " << swFrame.elapsed();
    //lock.lockForWrite(); //stop GL from updating while we calculate a new position

    // Phase 6.0.21: Calculate Hz from CPU timer (AgIOService.nowHz/gpsHz removed)
    // GPS frequency is calculated from frame timing
    nowHz = 1000.0 / swFrame.elapsed(); //convert ms into hz

    // Phase 6.0.20 Task 24 Step 5.6: Remove artificial 20 Hz ceiling in simulation mode
    // - Simulation mode: Show real CPU performance (can be 50+ Hz)
    // - Real mode fallback: Apply limits only if GPS disconnected
    // if (!SettingsManager::instance()->menu_isSimulatorOn()) {
    //     // Real mode fallback protection (GPS disconnected scenario)
    //     if (nowHz > 50) nowHz = 50;
    //     if (nowHz < 3) nowHz = 3;
    // }
    // Simulation mode: No limits, show true CPU performance

    //simple comp filter
    gpsHz = 0.98 * gpsHz + 0.02 * nowHz;

    //Initialization counter
    startCounter++;

    if (!isGPSPositionInitialized)
    {
        InitializeFirstFewGPSPositions();
        //lock.unlock();
        return;
    }

    //qDebug(qpos) << "Easting " <<  pn.fix.easting << "Northing" <<  pn.fix.northing << "Time " << swFrame.elapsed() << nowHz;

    swFrame.restart();

    pn.speed = pn.vtgSpeed;
    CVehicle::instance()->AverageTheSpeed(pn.speed);

    /*
    //GPS is valid, let's bootstrap the demo field if needed
    if(bootstrap_field)
    {
        fileCreateField();
        fileSaveABLines();
        bootstrap_field = false;
    }
    */

    //#region Heading
    //calculate current heading only when moving, otherwise use last
    if (headingFromSource == "Fix")
    {
        //#region Start

        distanceCurrentStepFixDisplay = glm::Distance(prevDistFix, pn.fix);
        double newDistance = this->distanceUser() + distanceCurrentStepFixDisplay;
        if (newDistance > 999) newDistance = 0;
        this->setDistanceUser(newDistance);
        distanceCurrentStepFixDisplay *= 100;

        prevDistFix = pn.fix;

        if (fabs(CVehicle::instance()->avgSpeed) < 1.5 && !isFirstHeadingSet)
            goto byPass;

        if (!isFirstHeadingSet) //set in steer settings, Stanley
        {
            prevFix.easting = stepFixPts[0].easting; prevFix.northing = stepFixPts[0].northing;

            if (stepFixPts[2].isSet == 0)
            {
                //this is the first position no roll or offset correction
                if (stepFixPts[0].isSet == 0)
                {
                    stepFixPts[0].easting = pn.fix.easting;
                    stepFixPts[0].northing = pn.fix.northing;
                    stepFixPts[0].isSet = 1;
                    //lock.unlock();
                    return;
                }

                //and the second
                if (stepFixPts[1].isSet == 0)
                {
                    for (int i = totalFixSteps - 1; i > 0; i--) stepFixPts[i] = stepFixPts[i - 1];
                    stepFixPts[0].easting = pn.fix.easting;
                    stepFixPts[0].northing = pn.fix.northing;
                    stepFixPts[0].isSet = 1;
                    //lock.unlock();
                    return;
                }

                //the critcal moment for checking initial direction/heading.
                for (int i = totalFixSteps - 1; i > 0; i--) stepFixPts[i] = stepFixPts[i - 1];
                stepFixPts[0].easting = pn.fix.easting;
                stepFixPts[0].northing = pn.fix.northing;
                stepFixPts[0].isSet = 1;

                setGpsHeading(atan2(pn.fix.easting - stepFixPts[2].easting,
                                    pn.fix.northing - stepFixPts[2].northing));

                if (gpsHeading() < 0) setGpsHeading(gpsHeading() + glm::twoPI);
                else if (gpsHeading() > glm::twoPI) setGpsHeading(gpsHeading() - glm::twoPI);

                CVehicle::instance()->fixHeading = gpsHeading();

                //set the imu to gps heading offset
                if (ahrs.imuHeading != 99999)
                {
                    double imuHeading = (glm::toRadians(ahrs.imuHeading));
                    imuGPS_Offset = 0;

                    //Difference between the IMU heading and the GPS heading
                    double gyroDelta = (imuHeading + imuGPS_Offset) - gpsHeading();

                    if (gyroDelta < 0) gyroDelta += glm::twoPI;
                    else if (gyroDelta > glm::twoPI) gyroDelta -= glm::twoPI;

                    //calculate delta based on circular data problem 0 to 360 to 0, clamp to +- 2 Pi
                    if (gyroDelta >= -glm::PIBy2 && gyroDelta <= glm::PIBy2) gyroDelta *= -1.0;
                    else
                    {
                        if (gyroDelta > glm::PIBy2) { gyroDelta = glm::twoPI - gyroDelta; }
                        else { gyroDelta = (glm::twoPI + gyroDelta) * -1.0; }
                    }
                    if (gyroDelta > glm::twoPI) gyroDelta -= glm::twoPI;
                    else if (gyroDelta < -glm::twoPI) gyroDelta += glm::twoPI;

                    //moe the offset to line up imu with gps
                    imuGPS_Offset = (gyroDelta);
                    //rounding a floating point number doesn't make sense.
                    //imuGPS_Offset = Math.Round(imuGPS_Offset, 6);

                    if (imuGPS_Offset >= glm::twoPI) imuGPS_Offset -= glm::twoPI;
                    else if (imuGPS_Offset <= 0) imuGPS_Offset += glm::twoPI;

                    //determine the Corrected heading based on gyro and GPS
                    _imuCorrected = imuHeading + imuGPS_Offset;
                    if (_imuCorrected > glm::twoPI) _imuCorrected -= glm::twoPI;
                    else if (_imuCorrected < 0) _imuCorrected += glm::twoPI;

                    // Phase 6.0.24 Problem 18: Validate _imuCorrected before assigning to fixHeading
                    if (std::isfinite(_imuCorrected) && fabs(_imuCorrected) < 100.0) {
                        CVehicle::instance()->fixHeading = _imuCorrected;
                    } else {
                        qWarning() << "Invalid _imuCorrected value:" << _imuCorrected << "- not assigned to fixHeading";
                    }
                }

                //set the camera
                camera.camHeading = glm::toDegrees(gpsHeading());

                //now we have a heading, fix the first 3
                if (CVehicle::instance()->antennaOffset != 0)
                {
                    for (int i = 0; i < 3; i++)
                    {
                        stepFixPts[i].easting = (cos(-gpsHeading()) * CVehicle::instance()->antennaOffset) + stepFixPts[i].easting;
                        stepFixPts[i].northing = (sin(-gpsHeading()) * CVehicle::instance()->antennaOffset) + stepFixPts[i].northing;
                    }
                }

                if (ahrs.imuRoll != 88888)
                {
                    // PHASE 6.0.31: Fixed tan() → sin() for geometric correctness
                    // Roll correction is horizontal displacement = height × sin(roll), NOT tan(roll)
                    // tan() causes exponential error for large angles (15.5% error at 30°)
                    rollCorrectionDistance = sin(glm::toRadians((ahrs.imuRoll))) * -CVehicle::instance()->antennaHeight;

                    // roll to left is positive  **** important!!
                    // not any more - April 30, 2019 - roll to right is positive Now! Still Important
                    for (int i = 0; i < 3; i++)
                    {
                        stepFixPts[i].easting = (cos(-gpsHeading()) * rollCorrectionDistance) + stepFixPts[i].easting;
                        stepFixPts[i].northing = (sin(-gpsHeading()) * rollCorrectionDistance) + stepFixPts[i].northing;
                    }
                }

                //get the distance from first to 2nd point, update fix with new offset/roll
                stepFixPts[0].distance = glm::Distance(stepFixPts[1], stepFixPts[0]);
                pn.fix.easting = stepFixPts[0].easting;
                pn.fix.northing = stepFixPts[0].northing;

                isFirstHeadingSet = true;
                TimedMessageBox(2000, "Direction Reset", "Forward is Set");

                lastGPS = pn.fix;

                //lock.unlock();
                return;
            }
        }
        //#endregion

        //#region Offset Roll

        // PHASE 6.0.33 FIX: Copy RAW GPS position from member FIRST (prevents cascade corrections)
        // m_rawGpsPosition set by onNmeaDataReady() at 8 Hz (never modified by corrections)
        // UpdateFixPosition() called at 50 Hz → always starts from same raw position
        {
            QMutexLocker lock(&m_rawGpsPositionMutex);
            rawGpsPosition = m_rawGpsPosition;
        }

        // PHASE 6.0.33 FIX: RESET pn.fix to RAW position BEFORE applying corrections
        // This ensures corrections are applied to FRESH base position, not accumulated
        pn.fix.easting = rawGpsPosition.easting;
        pn.fix.northing = rawGpsPosition.northing;

        // Apply antenna offset correction
        if (CVehicle::instance()->antennaOffset != 0)
        {
            pn.fix.easting = (cos(-gpsHeading()) * CVehicle::instance()->antennaOffset) + pn.fix.easting;
            pn.fix.northing = (sin(-gpsHeading()) * CVehicle::instance()->antennaOffset) + pn.fix.northing;
        }

        uncorrectedEastingGraph = pn.fix.easting;

        // Apply roll correction
        if (ahrs.imuRoll != 88888)
        {
            //change for roll to the right is positive times -1
            rollCorrectionDistance = sin(glm::toRadians((ahrs.imuRoll))) * -CVehicle::instance()->antennaHeight;
            correctionDistanceGraph = rollCorrectionDistance;

            pn.fix.easting = (cos(-gpsHeading()) * rollCorrectionDistance) + pn.fix.easting;
            pn.fix.northing = (sin(-gpsHeading()) * rollCorrectionDistance) + pn.fix.northing;
        }

        //#endregion

        //#region Fix Heading

        double minFixHeadingDistSquared;
        double newGPSHeading;
        double imuHeading;
        double camDelta;
        double gyroDelta;

        //imu on board
        if (ahrs.imuHeading != 99999)
        {
            //check for out-of bounds fusion weights in case config
            //file was edited and changed inappropriately.
            //TODO move this sort of thing to FormGPS::load_settings
            if (ahrs.fusionWeight > 0.4) ahrs.fusionWeight = 0.4;
            if (ahrs.fusionWeight < 0.2) ahrs.fusionWeight = 0.2;

            // PHASE 6.0.35: Always calculate IMU heading (even during GPS bypass)
            // This ensures fixHeading is updated continuously via IMU, preventing wheel shake
            imuHeading = (glm::toRadians(ahrs.imuHeading));

            //how far since last fix
            distanceCurrentStepFix = glm::Distance(stepFixPts[0], pn.fix);

            // PHASE 6.0.35: Recalculate GPS heading ONLY if distance sufficient
            // C# original: GPS heading update is conditional, but IMU fusion ALWAYS runs
            if (distanceCurrentStepFix >= gpsMinimumStepDistance)
            {
                //userDistance can be reset

                minFixHeadingDistSquared = minHeadingStepDist * minHeadingStepDist;
                fixToFixHeadingDistance = 0;

                for (int i = 0; i < totalFixSteps; i++)
                {
                    fixToFixHeadingDistance = glm::DistanceSquared(stepFixPts[i], pn.fix);
                    currentStepFix = i;

                    if (fixToFixHeadingDistance > minFixHeadingDistSquared)
                    {
                        break;
                    }
                }

                if (fixToFixHeadingDistance >= (minFixHeadingDistSquared * 0.5))
                {
                    newGPSHeading = atan2(pn.fix.easting - stepFixPts[currentStepFix].easting,
                                          pn.fix.northing - stepFixPts[currentStepFix].northing);
                    if (newGPSHeading < 0) newGPSHeading += glm::twoPI;

                    if (ahrs.isReverseOn)
                    {
                        ////what is angle between the last valid heading before stopping and one just now
                        delta = fabs(M_PI - fabs(fabs(newGPSHeading - _imuCorrected) - M_PI));

                        //ie change in direction
                        if (delta > 1.57) //
                        {
                            CVehicle::instance()->setIsReverse(true);
                            newGPSHeading += M_PI;
                            if (newGPSHeading < 0) newGPSHeading += glm::twoPI;
                            else if (newGPSHeading >= glm::twoPI) newGPSHeading -= glm::twoPI;
                            setIsReverseWithIMU(true);
                        }
                        else
                        {
                            CVehicle::instance()->setIsReverse(false);
                            setIsReverseWithIMU(false);
                        }
                    }
                    else
                    {
                        CVehicle::instance()->setIsReverse(false);
                    }

                    // PHASE 6.0.35: Wheel angle compensation (forwardComp/reverseComp already implemented)
                    if (CVehicle::instance()->isReverse())
                        newGPSHeading -= glm::toRadians(CVehicle::instance()->antennaPivot / 1
                                                        * mc.actualSteerAngleDegrees * ahrs.reverseComp);
                    else
                        newGPSHeading -= glm::toRadians(CVehicle::instance()->antennaPivot / 1
                                                        * mc.actualSteerAngleDegrees * ahrs.forwardComp);

                    if (newGPSHeading < 0) newGPSHeading += glm::twoPI;
                    else if (newGPSHeading >= glm::twoPI) newGPSHeading -= glm::twoPI;

                    setGpsHeading(newGPSHeading);

                    // PHASE 6.0.35 FIX: Update stepFixPts ONLY when GPS heading recalculated
                    // This ensures stepFixPts[0] and pn.fix remain spaced apart (critical for low speed)
                    // C# original: stepFixPts updated BEFORE byPass label, not after
                    for (int i = totalFixSteps - 1; i > 0; i--) stepFixPts[i] = stepFixPts[i - 1];
                    stepFixPts[0].easting = rawGpsPosition.easting;
                    stepFixPts[0].northing = rawGpsPosition.northing;
                    stepFixPts[0].isSet = 1;

                    //#region IMU Fusion - Update GPS->IMU offset

                    // IMU Fusion with heading correction, add the correction
                    //Difference between the IMU heading and the GPS heading
                    gyroDelta = 0;

                    //if (!isReverseWithIMU)
                    gyroDelta = (imuHeading + imuGPS_Offset) - gpsHeading();
                    //else
                    //{
                    //    gyroDelta = 0;
                    //}

                    if (gyroDelta < 0) gyroDelta += glm::twoPI;
                    else if (gyroDelta > glm::twoPI) gyroDelta -= glm::twoPI;

                    //calculate delta based on circular data problem 0 to 360 to 0, clamp to +- 2 Pi
                    if (gyroDelta >= -glm::PIBy2 && gyroDelta <= glm::PIBy2) gyroDelta *= -1.0;
                    else
                    {
                        if (gyroDelta > glm::PIBy2) { gyroDelta = glm::twoPI - gyroDelta; }
                        else { gyroDelta = (glm::twoPI + gyroDelta) * -1.0; }
                    }
                    if (gyroDelta > glm::twoPI) gyroDelta -= glm::twoPI;
                    else if (gyroDelta < -glm::twoPI) gyroDelta += glm::twoPI;

                    //move the offset to line up imu with gps
                    if(!isReverseWithIMU())
                        imuGPS_Offset += (gyroDelta * (ahrs.fusionWeight));
                    else
                        imuGPS_Offset += (gyroDelta * (0.02));

                    if (imuGPS_Offset > glm::twoPI) imuGPS_Offset -= glm::twoPI;
                    else if (imuGPS_Offset < 0) imuGPS_Offset += glm::twoPI;

                    //#endregion
                }
                // ELSE: fixToFixHeadingDistance too small -> keep existing gpsHeading and imuGPS_Offset
            }
            // ELSE: distanceCurrentStepFix too small -> keep existing gpsHeading and imuGPS_Offset

            // PHASE 6.0.35: ALWAYS calculate imuCorrected and update fixHeading (even during GPS bypass)
            // This is THE FIX: fixHeading updated at IMU rate (10 Hz) even when GPS heading stale
            // Result: No wheel shake at startup, no rotation jump when movement begins
            //determine the Corrected heading based on gyro and GPS
            _imuCorrected = imuHeading + imuGPS_Offset;
            if (_imuCorrected > glm::twoPI) _imuCorrected -= glm::twoPI;
            else if (_imuCorrected < 0) _imuCorrected += glm::twoPI;

            //use imu as heading when going slow
            // Phase 6.0.24 Problem 18: Validate _imuCorrected before assigning to fixHeading
            if (std::isfinite(_imuCorrected) && fabs(_imuCorrected) < 100.0) {
                CVehicle::instance()->fixHeading = _imuCorrected;
            } else {
                qWarning() << "Invalid _imuCorrected value:" << _imuCorrected << "- not assigned to fixHeading";
            }

            //#endregion
        }
        else
        {
            //how far since last fix
            distanceCurrentStepFix = glm::Distance(stepFixPts[0], pn.fix);

            // PHASE 6.0.35: Recalculate GPS heading ONLY if distance sufficient
            // No IMU available, so fixHeading = gpsHeading (updated only when GPS moves enough)
            if (distanceCurrentStepFix >= gpsMinimumStepDistance)
            {
                minFixHeadingDistSquared = minHeadingStepDist * minHeadingStepDist;
                fixToFixHeadingDistance = 0;

                for (int i = 0; i < totalFixSteps; i++)
                {
                    // PHASE 6.0.32: Use RAW position for distance check (consistent with heading calc)
                    fixToFixHeadingDistance = glm::DistanceSquared(stepFixPts[i], rawGpsPosition);
                    currentStepFix = i;

                    if (fixToFixHeadingDistance > minFixHeadingDistSquared)
                    {
                        break;
                    }
                }

                if (fixToFixHeadingDistance >= minFixHeadingDistSquared * 0.5)
                {
                    // PHASE 6.0.32: Calculate heading from RAW GPS positions (not corrected)
                    // Old code used pn.fix which contains CORRECTED position → heading affected by roll
                    newGPSHeading = atan2(rawGpsPosition.easting - stepFixPts[currentStepFix].easting,
                                          rawGpsPosition.northing - stepFixPts[currentStepFix].northing);
                    if (newGPSHeading < 0) newGPSHeading += glm::twoPI;

                    if (ahrs.isReverseOn)
                    {

                        ////what is angle between the last valid heading before stopping and one just now
                        delta = fabs(M_PI - fabs(fabs(newGPSHeading - gpsHeading()) - M_PI));

                        filteredDelta = delta * 0.2 + filteredDelta * 0.8;

                        //filtered delta different then delta
                        if (fabs(filteredDelta - delta) > 0.5)
                        {
                            CVehicle::instance()->setIsChangingDirection(true);
                        }
                        else
                        {
                            CVehicle::instance()->setIsChangingDirection(false);
                        }

                        //we can't be sure if changing direction so do nothing
                        if (CVehicle::instance()->isChangingDirection())
                        {
                            // Skip heading update when changing direction (unstable)
                        }
                        else
                        {
                            //ie change in direction
                            if (filteredDelta > 1.57) //
                            {
                                CVehicle::instance()->setIsReverse(true);
                                newGPSHeading += M_PI;
                                if (newGPSHeading < 0) newGPSHeading += glm::twoPI;
                                else if (newGPSHeading >= glm::twoPI) newGPSHeading -= glm::twoPI;
                            }
                            else
                                CVehicle::instance()->setIsReverse(false);

                            // PHASE 6.0.35: Wheel angle compensation (forwardComp/reverseComp already implemented)
                            if (CVehicle::instance()->isReverse())
                                newGPSHeading -= glm::toRadians(CVehicle::instance()->antennaPivot / 1
                                                                * mc.actualSteerAngleDegrees * ahrs.reverseComp);
                            else
                                newGPSHeading -= glm::toRadians(CVehicle::instance()->antennaPivot / 1
                                                                * mc.actualSteerAngleDegrees * ahrs.forwardComp);

                            if (newGPSHeading < 0) newGPSHeading += glm::twoPI;
                            else if (newGPSHeading >= glm::twoPI) newGPSHeading -= glm::twoPI;

                            //set the headings
                            setGpsHeading(newGPSHeading);
                            CVehicle::instance()->fixHeading = gpsHeading();

                            // PHASE 6.0.35 FIX: Update stepFixPts when heading recalculated (No IMU reverse path)
                            for (int i = totalFixSteps - 1; i > 0; i--) stepFixPts[i] = stepFixPts[i - 1];
                            stepFixPts[0].easting = rawGpsPosition.easting;
                            stepFixPts[0].northing = rawGpsPosition.northing;
                            stepFixPts[0].isSet = 1;
                        }
                    }

                    else
                    {
                        CVehicle::instance()->setIsReverse(false);

                        // PHASE 6.0.35 FIX: Apply wheel angle compensation in forward mode too!
                        // Bug: compensation was only applied in reverse detection branch
                        newGPSHeading -= glm::toRadians(CVehicle::instance()->antennaPivot / 1
                                                        * mc.actualSteerAngleDegrees * ahrs.forwardComp);

                        if (newGPSHeading < 0) newGPSHeading += glm::twoPI;
                        else if (newGPSHeading >= glm::twoPI) newGPSHeading -= glm::twoPI;

                        //set the headings
                        setGpsHeading(newGPSHeading);
                        CVehicle::instance()->fixHeading = gpsHeading();

                        // PHASE 6.0.35 FIX: Update stepFixPts when heading recalculated (No IMU forward path)
                        for (int i = totalFixSteps - 1; i > 0; i--) stepFixPts[i] = stepFixPts[i - 1];
                        stepFixPts[0].easting = rawGpsPosition.easting;
                        stepFixPts[0].northing = rawGpsPosition.northing;
                        stepFixPts[0].isSet = 1;
                    }
                }
                // ELSE: fixToFixHeadingDistance too small -> keep existing gpsHeading and fixHeading
            }
            // ELSE: distanceCurrentStepFix too small -> keep existing gpsHeading and fixHeading
        }

        // PHASE 6.0.35 FIX: stepFixPts update moved BEFORE afterByPass label (see lines 355-361, 505-509, 523-527)
        // This ensures stepFixPts[0] only updated when heading recalculated → fixes low-speed heading update bug

        //#endregion

        //#region Camera

        camDelta = CVehicle::instance()->fixHeading - smoothCamHeading;

        if (camDelta < 0) camDelta += glm::twoPI;
        else if (camDelta > glm::twoPI) camDelta -= glm::twoPI;

        //calculate delta based on circular data problem 0 to 360 to 0, clamp to +- 2 Pi
        if (camDelta >= -glm::PIBy2 && camDelta <= glm::PIBy2) camDelta *= -1.0;
        else
        {
            if (camDelta > glm::PIBy2) { camDelta = glm::twoPI - camDelta; }
            else { camDelta = (glm::twoPI + camDelta) * -1.0; }
        }
        if (camDelta > glm::twoPI) camDelta -= glm::twoPI;
        else if (camDelta < -glm::twoPI) camDelta += glm::twoPI;

        smoothCamHeading -= camDelta * camera.camSmoothFactor;

        if (smoothCamHeading > glm::twoPI) smoothCamHeading -= glm::twoPI;
        else if (smoothCamHeading < -glm::twoPI) smoothCamHeading += glm::twoPI;

        camera.camHeading = glm::toDegrees(smoothCamHeading);

        // PHASE 6.0.35 FIX: Skip byPass in normal flow (heading with wheel compensation already calculated)
        // byPass should only execute when jumped to from line 98 (slow speed / no initial heading)
        // Without this goto, byPass overwrites fixHeading (with wheel comp) → causes crab motion!
        goto afterByPass;

        //#endregion


        //Calculate a million other things
    byPass:
        if (ahrs.imuHeading != 99999)
        {
            _imuCorrected = (glm::toRadians(ahrs.imuHeading)) + imuGPS_Offset;
            if (_imuCorrected > glm::twoPI) _imuCorrected -= glm::twoPI;
            else if (_imuCorrected < 0) _imuCorrected += glm::twoPI;

            //use imu as heading when going slow
            // Phase 6.0.24 Problem 18: Validate _imuCorrected before assigning to fixHeading
            if (std::isfinite(_imuCorrected) && fabs(_imuCorrected) < 100.0) {
                CVehicle::instance()->fixHeading = _imuCorrected;
            } else {
                qWarning() << "Invalid _imuCorrected value:" << _imuCorrected << "- not assigned to fixHeading";
            }
        }

        camDelta = CVehicle::instance()->fixHeading - smoothCamHeading;

        if (camDelta < 0) camDelta += glm::twoPI;
        else if (camDelta > glm::twoPI) camDelta -= glm::twoPI;

        //calculate delta based on circular data problem 0 to 360 to 0, clamp to +- 2 Pi
        if (camDelta >= -glm::PIBy2 && camDelta <= glm::PIBy2) camDelta *= -1.0;
        else
        {
            if (camDelta > glm::PIBy2) { camDelta = glm::twoPI - camDelta; }
            else { camDelta = (glm::twoPI + camDelta) * -1.0; }
        }
        if (camDelta > glm::twoPI) camDelta -= glm::twoPI;
        else if (camDelta < -glm::twoPI) camDelta += glm::twoPI;

        smoothCamHeading -= camDelta * camera.camSmoothFactor;

        if (smoothCamHeading > glm::twoPI) smoothCamHeading -= glm::twoPI;
        else if (smoothCamHeading < -glm::twoPI) smoothCamHeading += glm::twoPI;

        camera.camHeading = glm::toDegrees(smoothCamHeading);

        afterByPass:
        TheRest();
    } else if (headingFromSource == "VTG")
    {
        isFirstHeadingSet = true;
        if (CVehicle::instance()->avgSpeed > 1)
        {
            //use NMEA headings for camera and tractor graphic
            CVehicle::instance()->fixHeading = glm::toRadians(pn.headingTrue);
            camera.camHeading = pn.headingTrue;
            setGpsHeading(CVehicle::instance()->fixHeading);
        }

        //grab the most current fix to last fix distance
        distanceCurrentStepFix = glm::Distance(pn.fix, prevFix);

        //#region Antenna Offset

        if (CVehicle::instance()->antennaOffset != 0)
        {
            pn.fix.easting = (cos(-CVehicle::instance()->fixHeading) * CVehicle::instance()->antennaOffset) + pn.fix.easting;
            pn.fix.northing = (sin(-CVehicle::instance()->fixHeading) * CVehicle::instance()->antennaOffset) + pn.fix.northing;
        }
        //#endregion

        uncorrectedEastingGraph = pn.fix.easting;

        //an IMU with heading correction, add the correction
        if (ahrs.imuHeading != 99999)
        {
            //current gyro angle in radians
            double correctionHeading = (glm::toRadians(ahrs.imuHeading));

            //Difference between the IMU heading and the GPS heading
            double gyroDelta = (correctionHeading + imuGPS_Offset) - gpsHeading();
            if (gyroDelta < 0) gyroDelta += glm::twoPI;

            //calculate delta based on circular data problem 0 to 360 to 0, clamp to +- 2 Pi
            if (gyroDelta >= -glm::PIBy2 && gyroDelta <= glm::PIBy2) gyroDelta *= -1.0;
            else
            {
                if (gyroDelta > glm::PIBy2) { gyroDelta = glm::twoPI - gyroDelta; }
                else { gyroDelta = (glm::twoPI + gyroDelta) * -1.0; }
            }
            if (gyroDelta > glm::twoPI) gyroDelta -= glm::twoPI;
            if (gyroDelta < -glm::twoPI) gyroDelta += glm::twoPI;

            //if the gyro and last corrected fix is < 10 degrees, super low pass for gps
            if (fabs(gyroDelta) < 0.18)
            {
                //a bit of delta and add to correction to current gyro
                imuGPS_Offset += (gyroDelta * (0.1));
                if (imuGPS_Offset > glm::twoPI) imuGPS_Offset -= glm::twoPI;
                if (imuGPS_Offset < -glm::twoPI) imuGPS_Offset += glm::twoPI;
            }
            else
            {
                //a bit of delta and add to correction to current gyro
                imuGPS_Offset += (gyroDelta * (0.2));
                if (imuGPS_Offset > glm::twoPI) imuGPS_Offset -= glm::twoPI;
                if (imuGPS_Offset < -glm::twoPI) imuGPS_Offset += glm::twoPI;
            }

            //determine the Corrected heading based on gyro and GPS
            _imuCorrected = correctionHeading + imuGPS_Offset;
            if (_imuCorrected > glm::twoPI) _imuCorrected -= glm::twoPI;
            if (_imuCorrected < 0) _imuCorrected += glm::twoPI;

            // Phase 6.0.24 Problem 18: Validate _imuCorrected before assigning to fixHeading
            if (std::isfinite(_imuCorrected) && fabs(_imuCorrected) < 100.0) {
                CVehicle::instance()->fixHeading = _imuCorrected;
            } else {
                qWarning() << "Invalid _imuCorrected value:" << _imuCorrected << "- not assigned to fixHeading";
            }

            camera.camHeading = CVehicle::instance()->fixHeading;
            if (camera.camHeading > glm::twoPI) camera.camHeading -= glm::twoPI;
            camera.camHeading = glm::toDegrees(camera.camHeading);
        }


        //#region Roll

        if (ahrs.imuRoll != 88888)
        {
            //change for roll to the right is positive times -1
            rollCorrectionDistance = sin(glm::toRadians((ahrs.imuRoll))) * -CVehicle::instance()->antennaHeight;
            correctionDistanceGraph = rollCorrectionDistance;

            // roll to left is positive  **** important!!
            // not any more - April 30, 2019 - roll to right is positive Now! Still Important
            pn.fix.easting = (cos(-CVehicle::instance()->fixHeading) * rollCorrectionDistance) + pn.fix.easting;
            pn.fix.northing = (sin(-CVehicle::instance()->fixHeading) * rollCorrectionDistance) + pn.fix.northing;
        }

        //#endregion Roll

        TheRest();

        //most recent fixes are now the prev ones
        prevFix.easting = pn.fix.easting; prevFix.northing = pn.fix.northing;

    } else if (headingFromSource == "Dual")
    {
        isFirstHeadingSet = true;
        //use Dual Antenna heading for camera and tractor graphic
        CVehicle::instance()->fixHeading = glm::toRadians(pn.headingTrueDual);
        setGpsHeading(CVehicle::instance()->fixHeading);

        uncorrectedEastingGraph = pn.fix.easting;

        if (CVehicle::instance()->antennaOffset != 0)
        {
            pn.fix.easting = (cos(-CVehicle::instance()->fixHeading) * CVehicle::instance()->antennaOffset) + pn.fix.easting;
            pn.fix.northing = (sin(-CVehicle::instance()->fixHeading) * CVehicle::instance()->antennaOffset) + pn.fix.northing;
        }

        if (ahrs.imuRoll != 88888 && CVehicle::instance()->antennaHeight != 0)
        {

            //change for roll to the right is positive times -1
            rollCorrectionDistance = sin(glm::toRadians((ahrs.imuRoll))) * -CVehicle::instance()->antennaHeight;
            correctionDistanceGraph = rollCorrectionDistance;

            // PHASE 6.0.35 FIX: Use fixHeading (not gpsHeading) for geometric consistency
            pn.fix.easting = (cos(-CVehicle::instance()->fixHeading) * rollCorrectionDistance) + pn.fix.easting;
            pn.fix.northing = (sin(-CVehicle::instance()->fixHeading) * rollCorrectionDistance) + pn.fix.northing;
        }

        //grab the most current fix and save the distance from the last fix
        distanceCurrentStepFix = glm::Distance(pn.fix, prevDistFix);

        //userDistance can be reset
        double userDistance = this->distanceUser() + distanceCurrentStepFix;
        if (userDistance > 999) userDistance = 0;
        this->setDistanceUser(userDistance);

        distanceCurrentStepFixDisplay = distanceCurrentStepFix * 100;
        prevDistFix = pn.fix;

        if (glm::DistanceSquared(lastReverseFix, pn.fix) > 0.20)
        {
            //most recent heading
            double newHeading = atan2(pn.fix.easting - lastReverseFix.easting,
                                      pn.fix.northing - lastReverseFix.northing);

            if (newHeading < 0) newHeading += glm::twoPI;


            //what is angle between the last reverse heading and current dual heading
            double delta = fabs(M_PI - fabs(fabs(newHeading - CVehicle::instance()->fixHeading) - M_PI));

            //are we going backwards
            CVehicle::instance()->setIsReverse(delta > 2 ? true : false);

            //save for next meter check
            lastReverseFix = pn.fix;
        }

        double camDelta = CVehicle::instance()->fixHeading - smoothCamHeading;

        if (camDelta < 0) camDelta += glm::twoPI;
        else if (camDelta > glm::twoPI) camDelta -= glm::twoPI;

        //calculate delta based on circular data problem 0 to 360 to 0, clamp to +- 2 Pi
        if (camDelta >= -glm::PIBy2 && camDelta <= glm::PIBy2) camDelta *= -1.0;
        else
        {
            if (camDelta > glm::PIBy2) { camDelta = glm::twoPI - camDelta; }
            else { camDelta = (glm::twoPI + camDelta) * -1.0; }
        }
        if (camDelta > glm::twoPI) camDelta -= glm::twoPI;
        else if (camDelta < -glm::twoPI) camDelta += glm::twoPI;

        smoothCamHeading -= camDelta * camera.camSmoothFactor;

        if (smoothCamHeading > glm::twoPI) smoothCamHeading -= glm::twoPI;
        else if (smoothCamHeading < -glm::twoPI) smoothCamHeading += glm::twoPI;

        camera.camHeading = glm::toDegrees(smoothCamHeading);

        TheRest();
    }
    //else {
    //}

    if (CVehicle::instance()->fixHeading >= glm::twoPI)
        CVehicle::instance()->fixHeading-= glm::twoPI;

    //#endregion
//
    //#region Corrected Position for GPS_OUT
    //NOTE: Michael, I'm not sure about this entire region

    double rollCorrectedLat;
    double rollCorrectedLon;
    // Phase 6.3.1: Use PropertyWrapper for safe QObject access
        pn.ConvertLocalToWGS84(pn.fix.northing, pn.fix.easting, rollCorrectedLat, rollCorrectedLon, this);

    QByteArray pgnRollCorrectedLatLon(22, 0);

    pgnRollCorrectedLatLon[0] = 0x80;
    pgnRollCorrectedLatLon[1] = 0x81;
    pgnRollCorrectedLatLon[2] = 0x7F;
    pgnRollCorrectedLatLon[3] = 0x64;
    pgnRollCorrectedLatLon[4] = 16;

    std::memcpy(pgnRollCorrectedLatLon.data() + 5, &rollCorrectedLon, 8);
    std::memcpy(pgnRollCorrectedLatLon.data() + 13, &rollCorrectedLat, 8);

    // SendPgnToLoop(pgnRollCorrectedLatLon); // ❌ REMOVED - Phase 4.6: AgIOService Workers handle PGN
    // GPS position data now flows through: AgIOService → FormGPS → pn/vehicle → OpenGL

    //#endregion

    //#region AutoSteer

    //preset the values
    CVehicle::instance()->guidanceLineDistanceOff = 32000;

    if (this->isContourBtnOn())
    {
        ct.DistanceFromContourLine(isBtnAutoSteerOn(), *CVehicle::instance(), yt, ahrs, pn, CVehicle::instance()->pivotAxlePos, CVehicle::instance()->steerAxlePos, mainWindow);
    }
    else
    {
        //auto track routine
        // PHASE 6.0.42.9: Fix auto-track condition (C# Position.designer.cs:826)
        // Added timer check to prevent rapid switching (max 1 switch/second)
        if (track.isAutoTrack() && !isBtnAutoSteerOn() && track.autoTrack3SecTimer >= 1)
        {
            track.autoTrack3SecTimer = 0;  // Reset timer after switch

            track.SwitchToClosestRefTrack(CVehicle::instance()->steerAxlePos, *CVehicle::instance());
        }

        bool autoSteerState = isBtnAutoSteerOn();
        track.BuildCurrentLine(CVehicle::instance()->pivotAxlePos,secondsSinceStart,autoSteerState,yt,*CVehicle::instance(),bnd,ahrs,gyd,pn);
    }

    // autosteer at full speed of updates

    //if the whole path driving driving process is green
    if (this->isDrivingRecordedPath()) recPath.UpdatePosition(*CVehicle::instance(), yt, isBtnAutoSteerOn());

    // If Drive button off - normal autosteer
    if (!CVehicle::instance()->isInFreeDriveMode)
    {
        //fill up0 the appropriate arrays with new values
        p_254.pgn[p_254.speedHi] = (char)((int)(fabs(CVehicle::instance()->avgSpeed) * 10.0) >> 8);
        p_254.pgn[p_254.speedLo] = (char)((int)(fabs(CVehicle::instance()->avgSpeed) * 10.0));
        //mc.machineControlData[mc.cnSpeed] = mc.autoSteerData[mc.sdSpeed];

        //save distance for display
        lightbarDistance = CVehicle::instance()->guidanceLineDistanceOff;

        if (!isBtnAutoSteerOn()) //32020 means auto steer is off
        {
            //NOTE: Is this supposed to be commented out?
            //CVehicle::instance()->guidanceLineDistanceOff = 32020;
            p_254.pgn[p_254.status] = 0;  // PHASE 6.0.29: OFF → send 0 (match C# original)
        }

        else p_254.pgn[p_254.status] = 1;  // PHASE 6.0.29: ON → send 1 (match C# original)

        if (this->isDrivingRecordedPath() || recPath.isFollowingDubinsToPath) p_254.pgn[p_254.status] = 1;  // PHASE 6.0.29: Force ON (match C# original)

        // PHASE 6.0.42.8: Auto-snap track to pivot when autosteer turns ON
        // C# original: OpenGL.Designer.cs:1858-1876
        // Behavior: When autosteer activates, automatically center track to current tractor position
        // This is a ONE-TIME snap (not continuous tracking) controlled by isAutoSnapped flag
        if (mc.steerSwitchHigh)
        {
            // Manual steer override active (switch on handlebar)
            // Reset auto-snap flag so it can snap again when autosteer re-enabled
            track.setIsAutoSnapped(false);
        }
        else if (isBtnAutoSteerOn())
        {
            // Autosteer is ON → perform auto-snap if enabled and not already snapped
            if (track.isAutoSnapToPivot() && !track.isAutoSnapped())
            {
                track.SnapToPivot();           // Nudge track to align with current pivot position
                track.setIsAutoSnapped(true);  // Mark as snapped (prevents re-snap until reset)
            }
        }
        else
        {
            // Autosteer is OFF → reset auto-snap flag for next activation cycle
            track.setIsAutoSnapped(false);
        }

        //mc.autoSteerData[7] = unchecked((byte)(CVehicle::instance()->guidanceLineDistanceOff >> 8));
        //mc.autoSteerData[8] = unchecked((byte)(CVehicle::instance()->guidanceLineDistanceOff));

        //convert to cm from mm and divide by 2 - lightbar
        int distanceX2;
        //if (CVehicle::instance()->guidanceLineDistanceOff == 32020 || CVehicle::instance()->guidanceLineDistanceOff == 32000)
        if (!isBtnAutoSteerOn() || CVehicle::instance()->guidanceLineDistanceOff == 32000)
            distanceX2 = 255;

        else
        {
            distanceX2 = (int)(CVehicle::instance()->guidanceLineDistanceOff * 0.05);

            if (distanceX2 < -127) distanceX2 = -127;
            else if (distanceX2 > 127) distanceX2 = 127;
            distanceX2 += 127;
        }

        p_254.pgn[p_254.lineDistance] = (char)distanceX2;

        if (!timerSim.isActive())
        {
            if (isBtnAutoSteerOn() && CVehicle::instance()->avgSpeed > CVehicle::instance()->maxSteerSpeed)
            {
                onStopAutoSteer();
                if (isMetric)
                    TimedMessageBox(3000, tr("AutoSteer Disabled"), tr("Above Maximum Safe Steering Speed: ") + locale.toString(CVehicle::instance()->maxSteerSpeed, 'g', 1) + tr(" Kmh"));
                else
                    TimedMessageBox(3000, tr("AutoSteer Disabled"), tr("Above Maximum Safe Steering Speed: ") + locale.toString(CVehicle::instance()->maxSteerSpeed * 0.621371, 'g', 1) + tr(" MPH"));
            }

            if (isBtnAutoSteerOn() && CVehicle::instance()->avgSpeed < CVehicle::instance()->minSteerSpeed)
            {
                minSteerSpeedTimer++;
                if (minSteerSpeedTimer > 80)
                {
                    onStopAutoSteer();
                    if (isMetric)
                        TimedMessageBox(3000, tr("AutoSteer Disabled"), tr("Below Minimum Safe Steering Speed: ") + locale.toString(CVehicle::instance()->minSteerSpeed, 'g', 1) + tr(" Kmh"));
                    else
                        TimedMessageBox(3000, tr("AutoSteer Disabled"), tr("Below Minimum Safe Steering Speed: ") + locale.toString(CVehicle::instance()->minSteerSpeed * 0.621371, 'g', 1) + tr(" MPH"));
                }
            }
            else
            {
                minSteerSpeedTimer = 0;
            }
        }

        double tanSteerAngle = tan(glm::toRadians(((double)(CVehicle::instance()->guidanceLineSteerAngle)) * 0.01));
        double tanActSteerAngle = tan(glm::toRadians(mc.actualSteerAngleDegrees));

        setAngVel = 0.277777 * CVehicle::instance()->avgSpeed * tanSteerAngle / CVehicle::instance()->wheelbase;
        actAngVel = glm::toDegrees(0.277777 * CVehicle::instance()->avgSpeed * tanActSteerAngle / CVehicle::instance()->wheelbase);


        isMaxAngularVelocity = false;
        //greater then settings rads/sec limit steer angle
        if (fabs(setAngVel) > CVehicle::instance()->maxAngularVelocity)
        {
            setAngVel = CVehicle::instance()->maxAngularVelocity;
            tanSteerAngle = 3.6 * setAngVel * CVehicle::instance()->wheelbase / CVehicle::instance()->avgSpeed;
            if (CVehicle::instance()->guidanceLineSteerAngle < 0)
                CVehicle::instance()->guidanceLineSteerAngle = (short)(glm::toDegrees(atan(tanSteerAngle)) * -100);
            else
                CVehicle::instance()->guidanceLineSteerAngle = (short)(glm::toDegrees(atan(tanSteerAngle)) * 100);
            isMaxAngularVelocity = true;
        }

        setAngVel = glm::toDegrees(setAngVel);

        p_254.pgn[p_254.steerAngleHi] = (char)(CVehicle::instance()->guidanceLineSteerAngle >> 8);
        p_254.pgn[p_254.steerAngleLo] = (char)(CVehicle::instance()->guidanceLineSteerAngle);

        if (CVehicle::instance()->isChangingDirection() && ahrs.imuHeading == 99999)
            p_254.pgn[p_254.status] = 0;  // PHASE 6.0.29: Changing direction → OFF (match C# original)

        //for now if backing up, turn off autosteer
        if (!isSteerInReverse)
        {
            if (CVehicle::instance()->isReverse()) p_254.pgn[p_254.status] = 0;  // PHASE 6.0.29: Reverse → OFF (match C# original)
        }
    }

    else //Drive button is on
    {
        //fill up the auto steer array with free drive values
        p_254.pgn[p_254.speedHi] = (char)((int)(80) >> 8);
        p_254.pgn[p_254.speedLo] = (char)((int)(80));

        //turn on status to operate
        p_254.pgn[p_254.status] = 1;  // PHASE 6.0.29: Free Drive ON (match C# original)

        //send the steer angle
        CVehicle::instance()->guidanceLineSteerAngle = (qint16)(CVehicle::instance()->driveFreeSteerAngle * 100);

        p_254.pgn[p_254.steerAngleHi] = (char)(CVehicle::instance()->guidanceLineSteerAngle >> 8);
        p_254.pgn[p_254.steerAngleLo] = (char)(CVehicle::instance()->guidanceLineSteerAngle);


    }

    //out serial to autosteer module  //indivdual classes load the distance and heading deltas
    // SendPgnToLoop(p_254.pgn); // ❌ REMOVED - Phase 4.6: AgIOService Workers handle PGN
    // Phase 6.0.33: Send PGN 254 at 50 Hz (synchronized with timer frequency)
    // Status byte (line 795) controls module behavior: 0=OFF (no steering), 1=ON (steering)
    // Module always responds with PGN 253 feedback → enables wheel display in real-time
    // SAFE: isBtnAutoSteerOn = false at startup (formgps.cpp:27), never saved in settings
    if (m_agioService) {
        m_agioService->sendPgn(p_254.pgn);
    }

    // Smart WAS Calibration data collection
    if (IsCollectingData && abs(CVehicle::instance()->guidanceLineDistanceOff) < 500) // Within 50cm of guidance line
    {
        // Convert guidanceLineSteerAngle from centidegrees to degrees and collect data
        AddSteerAngleSample(CVehicle::instance()->guidanceLineSteerAngle * 0.01, abs(CVehicle::instance()->avgSpeed));
    }

    //for average cross track error
    if (CVehicle::instance()->guidanceLineDistanceOff < 29000)
    {
        crossTrackError = (int)((double)crossTrackError * 0.90 + fabs((double)CVehicle::instance()->guidanceLineDistanceOff) * 0.1);
    }
    else
    {
        crossTrackError = 0;
    }

    //#endregion

    //#region AutoSteer

    //preset the values
    /*
     * NOTE: Can this all be removed? It's not present in CS
    CVehicle::instance()->guidanceLineDistanceOff = 32000;

    if (this->isContourBtnOn())
    {
        ct.DistanceFromContourLine(isBtnAutoSteerOn(), *CVehicle::instance(), yt, ahrs, pn, CVehicle::instance()->pivotAxlePos, CVehicle::instance()->steerAxlePos, mainWindow);
    }
    else
    {
        if (curve.isCurveSet && curve.isBtnCurveOn)
        {
            //do the calcs for AB Curve
            curve.GetCurrentCurveLine(CVehicle::instance()->pivotAxlePos, CVehicle::instance()->steerAxlePos, secondsSinceStart, isBtnAutoSteerOn(), mc.steerSwitchHigh, *CVehicle::instance(), bnd, yt, ahrs, gyd, pn);
        }

        if (ABLine.isABLineSet && ABLine.isBtnABLineOn)
        {
            ABLine.GetCurrentABLine(CVehicle::instance()->pivotAxlePos, CVehicle::instance()->steerAxlePos, secondsSinceStart, isBtnAutoSteerOn(), mc.steerSwitchHigh, *CVehicle::instance(), yt, ahrs, gyd, pn);
        }
    }

    // autosteer at full speed of updates

    //if the whole path driving driving process is green
    if (this->isDrivingRecordedPath()) recPath.UpdatePosition(*CVehicle::instance(), yt, isBtnAutoSteerOn());

    // If Drive button off - normal autosteer
    if (!CVehicle::instance()->isInFreeDriveMode)
    {
        //fill up0 the appropriate arrays with new values
        p_254.pgn[p_254.speedHi] = (char)((int)(fabs(CVehicle::instance()->avgSpeed) * 10.0) >> 8);
        p_254.pgn[p_254.speedLo] = (char)((int)(fabs(CVehicle::instance()->avgSpeed) * 10.0));
        //mc.machineControlData[mc.cnSpeed] = mc.autoSteerData[mc.sdSpeed];

        //save distance for display
        lightbarDistance = CVehicle::instance()->guidanceLineDistanceOff;

        if (!isBtnAutoSteerOn()) //32020 means auto steer is off
        {
            //CVehicle::instance()->guidanceLineDistanceOff = 32020;
            p_254.pgn[p_254.status] = 0;  // PHASE 6.0.29: OFF → send 0 (match C# original)
        }

        else p_254.pgn[p_254.status] = 1;  // PHASE 6.0.29: ON → send 1 (match C# original)

        if (this->isDrivingRecordedPath() || recPath.isFollowingDubinsToPath) p_254.pgn[p_254.status] = 1;  // PHASE 6.0.29: Force ON (match C# original)

        // PHASE 6.0.42.8: Auto-snap track to pivot when autosteer turns ON
        // C# original: OpenGL.Designer.cs:1858-1876
        // Behavior: When autosteer activates, automatically center track to current tractor position
        // This is a ONE-TIME snap (not continuous tracking) controlled by isAutoSnapped flag
        if (mc.steerSwitchHigh)
        {
            // Manual steer override active (switch on handlebar)
            // Reset auto-snap flag so it can snap again when autosteer re-enabled
            track.setIsAutoSnapped(false);
        }
        else if (isBtnAutoSteerOn())
        {
            // Autosteer is ON → perform auto-snap if enabled and not already snapped
            if (track.isAutoSnapToPivot() && !track.isAutoSnapped())
            {
                track.SnapToPivot();           // Nudge track to align with current pivot position
                track.setIsAutoSnapped(true);  // Mark as snapped (prevents re-snap until reset)
            }
        }
        else
        {
            // Autosteer is OFF → reset auto-snap flag for next activation cycle
            track.setIsAutoSnapped(false);
        }

        //mc.autoSteerData[7] = unchecked((byte)(CVehicle::instance()->guidanceLineDistanceOff >> 8));
        //mc.autoSteerData[8] = unchecked((byte)(CVehicle::instance()->guidanceLineDistanceOff));

        //convert to cm from mm and divide by 2 - lightbar
        int distanceX2;
        //if (CVehicle::instance()->guidanceLineDistanceOff == 32020 || CVehicle::instance()->guidanceLineDistanceOff == 32000)
        if (!isBtnAutoSteerOn() || CVehicle::instance()->guidanceLineDistanceOff == 32000)
            distanceX2 = 255;

        else
        {
            distanceX2 = (int)(CVehicle::instance()->guidanceLineDistanceOff * 0.05);

            if (distanceX2 < -127) distanceX2 = -127;
            else if (distanceX2 > 127) distanceX2 = 127;
            distanceX2 += 127;
        }

        p_254.pgn[p_254.lineDistance] = (char)distanceX2;

        if (!timerSim.isActive())
        {
            if (isBtnAutoSteerOn() && CVehicle::instance()->avgSpeed > CVehicle::instance()->maxSteerSpeed)
            {
                onStopAutoSteer();

                if (isMetric)
                    TimedMessageBox(3000, tr("AutoSteer Disabled"), tr("Above Maximum Safe Steering Speed: ") + locale.toString(CVehicle::instance()->maxSteerSpeed, 'g', 1) + tr(" Kmh"));
                else
                    TimedMessageBox(3000, tr("AutoSteer Disabled"), tr("Above Maximum Safe Steering Speed: ") + locale.toString(CVehicle::instance()->maxSteerSpeed * 0.621371, 'g', 1) + tr(" MPH"));
            }

            if (isBtnAutoSteerOn() && CVehicle::instance()->avgSpeed < CVehicle::instance()->minSteerSpeed)
            {
                minSteerSpeedTimer++;
                if (minSteerSpeedTimer > 80)
                {
                    onStopAutoSteer();
                    if (isMetric)
                        TimedMessageBox(3000, tr("AutoSteer Disabled"), tr("Below Minimum Safe Steering Speed: ") + locale.toString(CVehicle::instance()->maxSteerSpeed, 'g', 1) + tr(" Kmh"));
                    else
                        TimedMessageBox(3000, tr("AutoSteer Disabled"), tr("Below Minimum Safe Steering Speed: ") + locale.toString(CVehicle::instance()->maxSteerSpeed * 0.621371, 'g', 1) + tr(" MPH"));
                }
            }
            else
            {
                minSteerSpeedTimer = 0;
            }
        }

        double tanSteerAngle = tan(glm::toRadians(((double)(CVehicle::instance()->guidanceLineSteerAngle)) * 0.01));
        double tanActSteerAngle = tan(glm::toRadians(mc.actualSteerAngleDegrees));

        setAngVel = 0.277777 * CVehicle::instance()->avgSpeed * tanSteerAngle / CVehicle::instance()->wheelbase;
        actAngVel = glm::toDegrees(0.277777 * CVehicle::instance()->avgSpeed * tanActSteerAngle / CVehicle::instance()->wheelbase);


        isMaxAngularVelocity = false;
        //greater then settings rads/sec limit steer angle
        if (fabs(setAngVel) > CVehicle::instance()->maxAngularVelocity)
        {
            setAngVel = CVehicle::instance()->maxAngularVelocity;
            tanSteerAngle = 3.6 * setAngVel * CVehicle::instance()->wheelbase / CVehicle::instance()->avgSpeed;
            if (CVehicle::instance()->guidanceLineSteerAngle < 0)
                CVehicle::instance()->guidanceLineSteerAngle = (short)(glm::toDegrees(atan(tanSteerAngle)) * -100);
            else
                CVehicle::instance()->guidanceLineSteerAngle = (short)(glm::toDegrees(atan(tanSteerAngle)) * 100);
            isMaxAngularVelocity = true;
        }

        setAngVel = glm::toDegrees(setAngVel);

        p_254.pgn[p_254.steerAngleHi] = (char)(CVehicle::instance()->guidanceLineSteerAngle >> 8);
        p_254.pgn[p_254.steerAngleLo] = (char)(CVehicle::instance()->guidanceLineSteerAngle);

        if (isChangingDirection && ahrs.imuHeading == 99999)
            p_254.pgn[p_254.status] = 0;

        //for now if backing up, turn off autosteer
        if (!isSteerInReverse)
        {
            if (CVehicle::instance()->isReverse()) p_254.pgn[p_254.status] = 0;
        }
    }

    else //Drive button is on
    {
        //fill up the auto steer array with free drive values
        p_254.pgn[p_254.speedHi] = (char)((int)(80) >> 8);
        p_254.pgn[p_254.speedLo] = (char)((int)(80));

        //turn on status to operate
        p_254.pgn[p_254.status] = 1;

        //send the steer angle
        CVehicle::instance()->guidanceLineSteerAngle = (qint16)(CVehicle::instance()->driveFreeSteerAngle * 100);

        p_254.pgn[p_254.steerAngleHi] = (char)(CVehicle::instance()->guidanceLineSteerAngle >> 8);
        p_254.pgn[p_254.steerAngleLo] = (char)(CVehicle::instance()->guidanceLineSteerAngle);


    }

    //out serial to autosteer module  //indivdual classes load the distance and heading deltas
    // SendPgnToLoop(p_254.pgn); // ❌ REMOVED - Phase 4.6: AgIOService Workers handle PGN
    // Phase 6.0.33: Send PGN 254 at 50 Hz (synchronized with timer frequency)
    // Status byte (line 795) controls module behavior: 0=OFF (no steering), 1=ON (steering)
    // Module always responds with PGN 253 feedback → enables wheel display in real-time
    // SAFE: isBtnAutoSteerOn = false at startup (formgps.cpp:27), never saved in settings
    if (m_agioService) {
        m_agioService->sendPgn(p_254.pgn);
    }

    //for average cross track error
    if (CVehicle::instance()->guidanceLineDistanceOff < 29000)
    {
        crossTrackError = (int)((double)crossTrackError * 0.90 + fabs((double)CVehicle::instance()->guidanceLineDistanceOff) * 0.1);
    }
    else
    {
        crossTrackError = 0;
    }

    //#endregion
*/
    //#region Youturn

    //if an outer boundary is set, then apply critical stop logic
    if (bnd.bndList.count() > 0)
    {
        //check if inside all fence
        if (!this->isYouTurnBtnOn())
        {
            this->setIsOutOfBounds(!bnd.IsPointInsideFenceArea(CVehicle::instance()->pivotAxlePos));
            // Qt 6.8 FIX: Removed redundant self-assignment that could cause binding loop
        }
        else //Youturn is on
        {
            bool isInTurnBounds = bnd.IsPointInsideTurnArea(CVehicle::instance()->pivotAxlePos) != -1;
            //Are we inside outer and outside inner all turn boundaries, no turn creation problems
            //if we are too much off track > 1.3m, kill the diagnostic creation, start again
            //if (!yt.isYouTurnTriggered)
            if (isInTurnBounds)
            {
                this->setIsOutOfBounds(false);
                this->setIsOutOfBounds(false);
                //now check to make sure we are not in an inner turn boundary - drive thru is ok
                if (yt.youTurnPhase != 10)
                {
                    if (crossTrackError > 1000)
                    {
                        yt.ResetCreatedYouTurn();
                    }
                    else
                    {
                        if (track.getMode() == TrackMode::AB)
                        {
                            yt.BuildABLineDubinsYouTurn(this, yt.isYouTurnRight,*CVehicle::instance(),bnd,
                                                        track,secondsSinceStart);
                        }
                        else
                        {
                            yt.BuildCurveDubinsYouTurn(yt.isYouTurnRight, CVehicle::instance()->pivotAxlePos,
                                                       *CVehicle::instance(),bnd,track,secondsSinceStart);
                        }
                    }

                    if (yt.uTurnStyle == 0 && yt.youTurnPhase == 10)
                    {
                        yt.SmoothYouTurn(6);
                    }
                    if (yt.isTurnCreationTooClose && !yt.turnTooCloseTrigger)
                    {
                        yt.turnTooCloseTrigger = true;
                        //if (sounds.isTurnSoundOn) sounds.sndUTurnTooClose.Play(); Implemented in QML
                    }
                }
                else if (yt.ytList.count() > 5)//wait to trigger the actual turn since its made and waiting
                {
                    //distance from current pivot to first point of youturn pattern
                    _distancePivotToTurnLine = glm::Distance(yt.ytList[5], CVehicle::instance()->pivotAxlePos);

                    //if ((_distancePivotToTurnLine <= 20.0) && (_distancePivotToTurnLine >= 18.0) && !yt.isYouTurnTriggered)

                    /* moved to QML
                    if (!sounds.isBoundAlarming)
                    {
                        if (sounds.isTurnSoundOn) sounds.sndBoundaryAlarm.Play();
                        sounds.isBoundAlarming = true;
                    }*/
                    //yt.YouTurnTrigger(track, *CVehicle::instance());
                    //if we are close enough to pattern, trigger.
                    if ((_distancePivotToTurnLine <= 1.0) && (_distancePivotToTurnLine >= 0) && !yt.isYouTurnTriggered)
                    {
                        yt.YouTurnTrigger(track, *CVehicle::instance());
                        //moved to QML
                        //sounds.isBoundAlarming = false;
                    }

                    //if (isBtnAutoSteerOn() && CVehicle::instance()->guidanceLineDistanceOff > 300 && !yt.isYouTurnTriggered)
                    //{
                    //    yt.ResetCreatedYouTurn();
                    //}
                }
            }
            else
            {
                if (!yt.isYouTurnTriggered)
                {
                    yt.ResetCreatedYouTurn();
                    this->setIsOutOfBounds(!bnd.IsPointInsideFenceArea(CVehicle::instance()->pivotAxlePos));
                    // Qt 6.8 FIX: Removed redundant self-assignment that could cause binding loop
                }

            }

            //}
            //// here is stop logic for out of bounds - in an inner or out the outer turn border.
            //else
            //{
            //    //this->setIsOutOfBounds(true);
            //    if (isBtnAutoSteerOn())
            //    {
            //        if (this->isYouTurnBtnOn())
            //        {
            //            yt.ResetCreatedYouTurn();
            //            //sim.stepDistance = 0 / 17.86;
            //        }
            //    }
            //    else
            //    {
            //        yt.isTurnCreationTooClose = false;
            //    }

            //}
        }
    }
    else
    {
        this->setIsOutOfBounds(false);
        this->setIsOutOfBounds(false);
    }

    //#endregion

    //update main window
    //oglMain.MakeCurrent();
    //oglMain.Refresh();

    if (isJobStarted()) {
        processSectionLookahead();

        //oglZoom_Paint();
        //processOverlapCount();
    }

    qDebug(qpos) << "Time before painting field: " << (float)swFrame.nsecsElapsed() / 1000000;
#if !defined(Q_OS_WINDOWS) //&& !defined(Q_OS_ANDROID)
    oglMain_Paint();
#endif

    //NOTE: Not sure here.
    //stop the timer and calc how long it took to do calcs and draw
    AOGRendererInSG *renderer = mainWindow->findChild<AOGRendererInSG *>("openglcontrol");
    // CRITICAL: Force OpenGL update in GUI thread to prevent threading violation
    if (renderer) {
        QMetaObject::invokeMethod(renderer, "update", Qt::DirectConnection);
    }
    qDebug(qpos) << "Time after painting field: " << (float)swFrame.nsecsElapsed() / 1000000;

    frameTimeRough = swFrame.elapsed();

    //if (frameTimeRough > 80) frameTimeRough = 80;

    // Phase 6.0.20: Qt 6.8 BINDABLE - use setter for automatic signal emission
    setFrameTime(frameTime() * 0.90 + frameTimeRough * 0.1);

    // ===== Q_PROPERTY OPTIMIZED UPDATE - OPTION A =====
    // Direct member updates + grouped signals (305x faster than setProperty)

    // Variables for change tracking
    bool posChangedFlag = false, vehChangedFlag = false, steerChangedFlag = false;
    bool imuChangedFlag = false, gpsChangedFlag = false, blockageChangedFlag = false;
    bool navChangedFlag = false, toolPosChangedFlag = false, wizardChangedFlag = false;
    bool geometryChangedFlag = false, miscChangedFlag = false;

    // Calculate tool position once
    double tool_lat, tool_lon;
    // Phase 6.3.1: Use PropertyWrapper for safe QObject access
        pn.ConvertLocalToWGS84(CVehicle::instance()->pivotAxlePos.northing, CVehicle::instance()->pivotAxlePos.easting, tool_lat, tool_lon, this);

    // Phase 6.0.20: Qt 6.8 BINDABLE - use setter for automatic signal emission
    setAvgPivDistance(avgPivDistance() * 0.5 + CVehicle::instance()->guidanceLineDistanceOff * 0.5);

    // Steer module counter logic - Phase 6.0.20 Task 24 Step 3.2
    if (!timerSim.isActive()) {
        int counter = steerModuleConnectedCounter();
        if (counter++ > 30)
            counter = 31;
        setSteerModuleConnectedCounter(counter);
    }

    // === Position GPS Updates (6 properties) - Qt 6.8 QProperty (Phase 6.0.9.06) ===
    if (m_latitude != pn.latitude) { m_latitude = pn.latitude; posChangedFlag = true; }
    if (m_longitude != pn.longitude) { m_longitude = pn.longitude; posChangedFlag = true; }
    if (m_altitude != pn.altitude) { m_altitude = pn.altitude; posChangedFlag = true; }
    if (m_easting != CVehicle::instance()->pivotAxlePos.easting) { m_easting = CVehicle::instance()->pivotAxlePos.easting; posChangedFlag = true; }
    if (m_northing != CVehicle::instance()->pivotAxlePos.northing) { m_northing = CVehicle::instance()->pivotAxlePos.northing; posChangedFlag = true; }
    if (m_heading != CVehicle::instance()->pivotAxlePos.heading) { m_heading = CVehicle::instance()->pivotAxlePos.heading; posChangedFlag = true; }

    // === Vehicle State Updates (8 properties) ===
    if (m_speedKph != CVehicle::instance()->avgSpeed) { m_speedKph = CVehicle::instance()->avgSpeed; vehChangedFlag = true; }
    if (m_fusedHeading != CVehicle::instance()->fixHeading) { m_fusedHeading = CVehicle::instance()->fixHeading; vehChangedFlag = true; }
    if (m_toolEasting != CVehicle::instance()->toolPos.easting) { m_toolEasting = CVehicle::instance()->toolPos.easting; vehChangedFlag = true; }
    if (m_toolNorthing != CVehicle::instance()->toolPos.northing) { m_toolNorthing = CVehicle::instance()->toolPos.northing; vehChangedFlag = true; }
    if (m_toolHeading != CVehicle::instance()->toolPos.heading) { m_toolHeading = CVehicle::instance()->toolPos.heading; vehChangedFlag = true; }
    if (m_offlineDistance != CVehicle::instance()->guidanceLineDistanceOff) {
        m_offlineDistance = CVehicle::instance()->guidanceLineDistanceOff;
        // Phase 6.0.20: Q_OBJECT_BINDABLE_PROPERTY auto-emits offlineDistanceChanged()
        // No manual qmlItem()->setProperty() needed - BINDABLE handles QML reactivity
        vehChangedFlag = true;
    }
    // avgPivDistance uses existing variable directly - no member needed
    // isReverseWithIMU now uses Q_OBJECT_BINDABLE_PROPERTY m_isReverseWithIMU

    // === Steering Control Updates (6 properties) ===
    if (m_steerAngleActual != mc.actualSteerAngleDegrees) { m_steerAngleActual = mc.actualSteerAngleDegrees; steerChangedFlag = true; }
    if (m_steerAngleSet != CVehicle::instance()->driveFreeSteerAngle) { m_steerAngleSet = CVehicle::instance()->driveFreeSteerAngle; steerChangedFlag = true; }
    if (m_lblPWMDisplay != mc.pwmDisplay) { m_lblPWMDisplay = mc.pwmDisplay; steerChangedFlag = true; }
    if (m_calcSteerAngleInner != steerAngleRight) { m_calcSteerAngleInner = steerAngleRight; steerChangedFlag = true; }
    if (m_calcSteerAngleOuter != steerAngleRight) { m_calcSteerAngleOuter = steerAngleRight; steerChangedFlag = true; }
    if (m_diameter != _diameter) { m_diameter = _diameter; steerChangedFlag = true; }

    // === IMU Data Updates (5 properties) ===
    if (m_imuRoll != ahrs.imuRoll) { m_imuRoll = ahrs.imuRoll; imuChangedFlag = true; }
    if (m_imuPitch != ahrs.imuPitch) { m_imuPitch = ahrs.imuPitch; imuChangedFlag = true; }
    if (m_imuHeading != ahrs.imuHeading) { m_imuHeading = ahrs.imuHeading; imuChangedFlag = true; }
    if (m_imuRollDegrees != ahrs.imuRoll) { m_imuRollDegrees = ahrs.imuRoll; imuChangedFlag = true; }
    if (m_imuAngVel != ahrs.angVel) { m_imuAngVel = ahrs.angVel; imuChangedFlag = true; }

    // === GPS Status Updates (8 properties) ===
    if (m_hdop != pn.hdop) { m_hdop = pn.hdop; gpsChangedFlag = true; }
    if (m_age != pn.age) { m_age = pn.age; gpsChangedFlag = true; }
    if (m_fixQuality != (int)pn.fixQuality) { m_fixQuality = (int)pn.fixQuality; gpsChangedFlag = true; }
    if (m_satellitesTracked != pn.satellitesTracked) { m_satellitesTracked = pn.satellitesTracked; gpsChangedFlag = true; }
    if (m_hz != gpsHz) { m_hz = gpsHz; gpsChangedFlag = true; }
    if (m_rawHz != nowHz) { m_rawHz = nowHz; gpsChangedFlag = true; }
    // Phase 6.0.20 Task 24 Step 5.6: droppedSentences - TODO implement real GPS frame drop counter
    // For now, set to 0 (old udpWatchCounts removed in Phase 4.6 AgIOService migration)
    if (m_droppedSentences != 0) { m_droppedSentences = 0; gpsChangedFlag = true; }
    // frameTime and steerModuleConnectedCounter use existing variables directly - no members needed

    // === Blockage Sensors Updates (8 properties) - Qt 6.8 QProperty ===
    if (m_blockage_avg != tool.blockage_avg) { m_blockage_avg = tool.blockage_avg; blockageChangedFlag = true; }
    if (m_blockage_min1 != tool.blockage_min1) { m_blockage_min1 = tool.blockage_min1; blockageChangedFlag = true; }
    if (m_blockage_min2 != tool.blockage_min2) { m_blockage_min2 = tool.blockage_min2; blockageChangedFlag = true; }
    if (m_blockage_max != tool.blockage_max) { m_blockage_max = tool.blockage_max; blockageChangedFlag = true; }
    if (m_blockage_min1_i != tool.blockage_min1_i) { m_blockage_min1_i = tool.blockage_min1_i; blockageChangedFlag = true; }
    if (m_blockage_min2_i != tool.blockage_min2_i) { m_blockage_min2_i = tool.blockage_min2_i; blockageChangedFlag = true; }
    if (m_blockage_max_i != tool.blockage_max_i) { m_blockage_max_i = tool.blockage_max_i; blockageChangedFlag = true; }
    if (m_blockage_blocked != (bool)tool.blockage_blocked) { m_blockage_blocked = (bool)tool.blockage_blocked; blockageChangedFlag = true; }

    // === Navigation Updates (6 properties) ===
    if (m_distancePivotToTurnLine != _distancePivotToTurnLine) { m_distancePivotToTurnLine = _distancePivotToTurnLine; navChangedFlag = true; }
    if (m_isYouTurnRight != yt.isYouTurnRight) { m_isYouTurnRight = yt.isYouTurnRight; navChangedFlag = true; }
    if (m_isYouTurnTriggered != yt.isYouTurnTriggered) { m_isYouTurnTriggered = yt.isYouTurnTriggered; navChangedFlag = true; }
    if (m_current_trackNum != track.getHowManyPathsAway()) { m_current_trackNum = track.getHowManyPathsAway(); navChangedFlag = true; }
    if (m_lblmodeActualXTE != CVehicle::instance()->modeActualXTE) { m_lblmodeActualXTE = CVehicle::instance()->modeActualXTE; navChangedFlag = true; }
    if (m_lblmodeActualHeadingError != CVehicle::instance()->modeActualHeadingError) { m_lblmodeActualHeadingError = CVehicle::instance()->modeActualHeadingError; navChangedFlag = true; }

    // === Tool Position Updates (2 properties) ===
    if (m_toolLatitude != tool_lat) { m_toolLatitude = tool_lat; toolPosChangedFlag = true; }
    if (m_toolLongitude != tool_lon) { m_toolLongitude = tool_lon; toolPosChangedFlag = true; }

    // === Wizard/Calibration Updates (4 properties) ===
    if (m_sampleCount != SampleCount) { m_sampleCount = SampleCount; wizardChangedFlag = true; }
    if (m_confidenceLevel != ConfidenceLevel) { m_confidenceLevel = ConfidenceLevel; wizardChangedFlag = true; }
    if (m_hasValidRecommendation != HasValidRecommendation) { m_hasValidRecommendation = HasValidRecommendation; wizardChangedFlag = true; }
    if (m_startSA != isSA) { m_startSA = isSA; wizardChangedFlag = true; }

    // === Visual Geometry Updates (2 properties) ===
    QVariant newVehicleXY = CVehicle::instance()->pivot_axle_xy;
    QVariant newBoundingBox = CVehicle::instance()->bounding_box;
    if (m_vehicle_xy != newVehicleXY) { m_vehicle_xy = newVehicleXY; geometryChangedFlag = true; }
    if (m_vehicle_bounding_box != newBoundingBox) { m_vehicle_bounding_box = newBoundingBox; geometryChangedFlag = true; }

    // === Misc Status Updates (2 properties) ===
    if (m_steerSwitchHigh != mc.steerSwitchHigh) { m_steerSwitchHigh = mc.steerSwitchHigh; miscChangedFlag = true; }
    if (m_imuCorrected != _imuCorrected) { m_imuCorrected = _imuCorrected; miscChangedFlag = true; }

    // ===== QProperty + BINDABLE AUTOMATIC NOTIFICATIONS =====
    // Qt 6.8 QProperty system automatically handles change notifications
    // Manual signal emissions removed to prevent binding loops and crashes
    // Performance: QProperty automatic notifications are optimized by Qt

    // Note: Change detection flags (posChangedFlag, vehChangedFlag, etc.)
    // are kept for potential future optimizations but not used for signals
}

void FormGPS::TheRest()
{
    //positions and headings
    CalculatePositionHeading();

    //calculate lookahead at full speed, no sentence misses
    CalculateSectionLookAhead(CVehicle::instance()->toolPos.northing, CVehicle::instance()->toolPos.easting, CVehicle::instance()->cosSectionHeading, CVehicle::instance()->sinSectionHeading);

    //To prevent drawing high numbers of triangles, determine and test before drawing vertex
    sectionTriggerDistance = glm::Distance(pn.fix, prevSectionPos);
    contourTriggerDistance = glm::Distance(pn.fix, prevContourPos);
    gridTriggerDistance = glm::DistanceSquared(pn.fix, prevGridPos);

    //NOTE: Michael, maybe verify this is all good
    if ( isLogElevation && gridTriggerDistance > 2.9 && patchCounter !=0 && isJobStarted())
    {
        //grab fix and elevation
        sbGrid.append(
            QString::number(pn.latitude, 'f', 7).toUtf8() + ","
            + QString::number(pn.longitude, 'f', 7).toUtf8() + ","
            + QString::number(pn.altitude - CVehicle::instance()->antennaHeight, 'f', 3).toUtf8() + ","
            + QString::number(pn.fixQuality).toUtf8() + ","
            + QString::number(pn.fix.easting, 'f', 2).toUtf8() + ","
            + QString::number(pn.fix.northing, 'f', 2).toUtf8() + ","
            + QString::number(CVehicle::instance()->pivotAxlePos.heading, 'f', 3).toUtf8() + ","
            + QString::number(ahrs.imuRoll, 'f', 3).toUtf8()
            + "\r\n");

        prevGridPos.easting = CVehicle::instance()->pivotAxlePos.easting;
        prevGridPos.northing = CVehicle::instance()->pivotAxlePos.northing;
    }

    //contour points
    if (isJobStarted() &&(contourTriggerDistance > tool.contourWidth
                         || contourTriggerDistance > sectionTriggerStepDistance))
    {
        AddContourPoints();
    }

    //section on off and points
    if (sectionTriggerDistance > sectionTriggerStepDistance && isJobStarted())
    {
        AddSectionOrPathPoints();
    }

    //test if travelled far enough for new boundary point
    if (bnd.isOkToAddPoints)
    {
        double boundaryDistance = glm::Distance(pn.fix, prevBoundaryPos);
        if (boundaryDistance > 1) AddBoundaryPoint();
    }

    //calc distance travelled since last GPS fix
    //distance = glm::distance(pn.fix, prevFix);
    //if (CVehicle::instance()->avgSpeed > 1)

    if ((CVehicle::instance()->avgSpeed - previousSpeed  ) < -CVehicle::instance()->panicStopSpeed && CVehicle::instance()->panicStopSpeed != 0)
    {
        if (isBtnAutoSteerOn()) onStopAutoSteer();
    }

    previousSpeed = CVehicle::instance()->avgSpeed;
}

void FormGPS::processSectionLookahead() {
    //qDebug(qpos) << "frame time before doing section lookahead " << swFrame.elapsed(;
    //lock.lockForWrite(;
    //qDebug(qpos) << "frame time after getting lock  " << swFrame.elapsed(;
#define USE_QPAINTER_BACKBUFFER

    qDebug(qpos) << "Main callback thread is" << QThread::currentThread();

#ifdef USE_QPAINTER_BACKBUFFER
    auto result = QtConcurrent::run( [this]() {
        QMatrix4x4 projection;
        QMatrix4x4 modelview;

        //  Load the identity.
        projection.setToIdentity();

        //projection.perspective(6.0f,1,1,6000);
        projection.perspective(glm::toDegrees((double)0.06f), 1.666666666666f, 50.0f, 520.0f);

        if (this->grnPix.isNull())
            this->grnPix = QImage(QSize(500,300), QImage::Format_RGBX8888);

        this->grnPix.fill(0);

        //gl->glLoadIdentity();					// Reset The View
        modelview.setToIdentity();

        //back the camera up
        modelview.translate(0, 0, -500);

        //rotate camera so heading matched fix heading in the world
        //gl->glRotated(toDegrees(CVehicle::instance()->fixHeadingSection), 0, 0, 1);
        modelview.rotate(glm::toDegrees(CVehicle::instance()->toolPos.heading), 0, 0, 1);

        modelview.translate(-CVehicle::instance()->toolPos.easting - sin(CVehicle::instance()->toolPos.heading) * 15,
                            -CVehicle::instance()->toolPos.northing - cos(CVehicle::instance()->toolPos.heading) * 15,
                            0);

        // Viewport: NDC to pixel coordinates
        QMatrix4x4 viewport;
        viewport.translate(500 / 2.0f, 300 / 2.0f, 0);
        viewport.scale(500 / 2.0f, -300 / 2.0f, 1);  // negative Y to flip

        QMatrix4x4 mvp = projection * modelview;


        //patch color
        QColor patchColor = QColor::fromRgbF(0.0f, 0.5f, 0.0f);

        QPainter painter;
        if (!painter.begin(&grnPix)) {
            qWarning() << "New GPS frame but back buffer painter is still working on the last one.";
            return;
        }

        painter.setRenderHint(QPainter::Antialiasing, false);

        painter.setPen(Qt::NoPen);

        QMatrix4x4 vmvp = viewport * mvp;

        painter.setTransform(vmvp.toTransform());
        painter.setBrush(QBrush(patchColor));

        QPolygonF triangle;
        QList<QLineF> lines;

        //to draw or not the triangle patch
        bool isDraw;

        double pivEplus = CVehicle::instance()->pivotAxlePos.easting + 50;
        double pivEminus = CVehicle::instance()->pivotAxlePos.easting - 50;
        double pivNplus = CVehicle::instance()->pivotAxlePos.northing + 50;
        double pivNminus = CVehicle::instance()->pivotAxlePos.northing - 50;

        //QPolygonF frustum({{pivEminus, pivNplus}, {pivEplus, pivNplus },
        //                   { pivEplus, pivNminus}, {pivEminus, pivNminus }});

        //draw patches j= # of sections
        for (int j = 0; j < this->triStrip.count(); j++)
        {
            //every time the section turns off and on is a new patch
            int patchCount = this->triStrip[j].patchList.size();

            if (patchCount > 0)
            {
                //for every new chunk of patch
                for (int k = 0; k < this->triStrip[j].patchList.size() ; k++)
                {
                    isDraw = false;
                    QSharedPointer<PatchTriangleList> triList = this->triStrip[j].patchList[k];
                    QSharedPointer<PatchBoundingBox> bb = this->triStrip[j].patchBoundingBoxList[k];

                    /*
                    QPolygonF patchBox({{ (*bb).minx, (*bb).miny }, {(*bb).maxx, (*bb).miny},
                                        { (*bb).maxx, (*bb).maxy }, { (*bb).minx, (*bb).maxy } });

                    if (frustum.intersects(patchBox))
                        isDraw = true;
                    */

                    int count2 = triList->size();
                    for (int i = 1; i < count2; i+=3)
                    {
                        //determine if point is in frustum or not
                        if ((*triList)[i].x() > pivEplus)
                            continue;
                        if ((*triList)[i].x() < pivEminus)
                            continue;
                        if ((*triList)[i].y() > pivNplus)
                            continue;
                        if ((*triList)[i].y() < pivNminus)
                            continue;

                        //point is in frustum so draw the entire patch
                        isDraw = true;
                        break;
                    }

                    if (isDraw)
                    {
                        triangle.clear();
                        //triangle strip to polygon:
                        //first two vertices, then every other one to the end
                        //then from the end back to vertex #3, but every other one.

                        triangle.append(QPointF((*triList)[1].x(), (*triList)[1].y()));
                        triangle.append(QPointF((*triList)[2].x(), (*triList)[2].y()));

                        //even vertices after first two
                        for (int i=4; i < count2; i+=2) {
                            triangle.append(QPointF((*triList)[i].x(), (*triList)[i].y()));
                        }

                        //odd remaining vertices
                        for (int i=count2 - (count2 % 2 ? 2 : 1) ; i >2 ; i -=2) {
                            triangle.append(QPointF((*triList)[i].x(), (*triList)[i].y()));
                        }

                        painter.drawPolygon(triangle);

                    }
                }
            }
        }

        //draw tool bar for debugging
        //gldraw.clear();
        //gldraw.append(QVector3D(tool.section[0].leftPoint.easting, tool.section[0].leftPoint.northing,0.5));
        //gldraw.append(QVector3D(tool.section[tool.numOfSections-1].rightPoint.easting, tool.section[tool.numOfSections-1].rightPoint.northing,0.5));
        //gldraw.draw(gl,projection*modelview,QColor::fromRgb(255,0,0),GL_LINE_STRIP,1);

        //draw 245 green for the tram tracks
        QPen pen(QColor::fromRgb(0,245,0));
        pen.setWidth(8);
        painter.setPen(pen);

        if (this->tram.displayMode !=0 && this->tram.displayMode !=0 && (this->track.idx() > -1))
        {
            if ((this->tram.displayMode == 1 || this->tram.displayMode == 2))
            {

                for (int i = 0; i < this->tram.tramList.count(); i++)
                {
                    lines.clear();
                    for (int h = 1; h < this->tram.tramList[i]->count(); h++) {
                        lines.append(QLineF(glm::backbuffer_world_to_screen(mvp, (*this->tram.tramList[i])[h-1]),
                                           glm::backbuffer_world_to_screen(mvp, (*this->tram.tramList[i])[h])));
                    }

                    painter.drawLines(lines);
                }
            }

            if (this->tram.displayMode == 1 || this->tram.displayMode == 3)
            {
                lines.clear();
                for (int h = 0; h < this->tram.tramBndOuterArr.count(); h++) {
                    lines.append(QLineF(glm::backbuffer_world_to_screen(mvp, this->tram.tramBndOuterArr[h-1]),
                                       glm::backbuffer_world_to_screen(mvp, this->tram.tramBndOuterArr[h])));
                }

                for (int h = 0; h < this->tram.tramBndInnerArr.count(); h++) {
                    lines.append(QLineF(glm::backbuffer_world_to_screen(mvp, this->tram.tramBndInnerArr[h-1]),
                                       glm::backbuffer_world_to_screen(mvp, this->tram.tramBndInnerArr[h])));
                }

                painter.drawLines(lines);
            }
        }

        //draw 240 green for boundary
        if (this->bnd.bndList.count() > 0)
        {
            ////draw the bnd line
            if (this->bnd.bndList[0].fenceLine.count() > 3)
            {
                DrawPolygonBack(painter, mvp, this->bnd.bndList[0].fenceLine,3,QColor::fromRgb(0,240,0));
            }


            //draw 250 green for the headland
            if (this->isHeadlandOn() && this->bnd.isSectionControlledByHeadland)
            {
                DrawPolygonBack(painter, mvp, this->bnd.bndList[0].hdLine,3,QColor::fromRgb(0,250,0));
            }
        }

        painter.end();

        //TODO adjust coordinate transformations above to eliminate this step
        this->grnPix = this->grnPix.mirrored().convertToFormat(QImage::Format_RGBX8888);

        QImage temp = this->grnPix.copy(tool.rpXPosition, 0, tool.rpWidth, 290 /*(int)rpHeight*/);
        temp.setPixelColor(0,0,QColor::fromRgb(255,128,0));
        //grnPix = temp; //only show clipped image
        memcpy(this->grnPixels, temp.constBits(), temp.size().width() * temp.size().height() * 4);
        //grnPix = temp;

        QThread *currentThread = QThread::currentThread();
        qDebug(qpos) << "Back render thread is" << currentThread;

        QMetaObject::invokeMethod(QApplication::instance(), [this]() {
#else
    oglBack_Paint();
#endif

    QThread *currentThread = QThread::currentThread();
    qDebug(qpos) << "Back processing thread is" << currentThread;

    if (SettingsManager::instance()->display_showBack()) {
        grnPixelsWindow->setPixmap(QPixmap::fromImage(grnPix.mirrored()));
        //overlapPixelsWindow->setPixmap(QPixmap::fromImage(overPix.mirrored()));
    }

    //determine where the tool is wrt to headland
    if (this->isHeadlandOn()) bnd.WhereAreToolCorners(tool);

    //set the look ahead for hyd Lift in pixels per second
    CVehicle::instance()->hydLiftLookAheadDistanceLeft = tool.farLeftSpeed * CVehicle::instance()->hydLiftLookAheadTime * 10;
    CVehicle::instance()->hydLiftLookAheadDistanceRight = tool.farRightSpeed * CVehicle::instance()->hydLiftLookAheadTime * 10;

    if (CVehicle::instance()->hydLiftLookAheadDistanceLeft > 200) CVehicle::instance()->hydLiftLookAheadDistanceLeft = 200;
    if (CVehicle::instance()->hydLiftLookAheadDistanceRight > 200) CVehicle::instance()->hydLiftLookAheadDistanceRight = 200;

    tool.lookAheadDistanceOnPixelsLeft = tool.farLeftSpeed * tool.lookAheadOnSetting * 10;
    tool.lookAheadDistanceOnPixelsRight = tool.farRightSpeed * tool.lookAheadOnSetting * 10;

    if (tool.lookAheadDistanceOnPixelsLeft > 200) tool.lookAheadDistanceOnPixelsLeft = 200;
    if (tool.lookAheadDistanceOnPixelsRight > 200) tool.lookAheadDistanceOnPixelsRight = 200;

    tool.lookAheadDistanceOffPixelsLeft = tool.farLeftSpeed * tool.lookAheadOffSetting * 10;
    tool.lookAheadDistanceOffPixelsRight = tool.farRightSpeed * tool.lookAheadOffSetting * 10;

    if (tool.lookAheadDistanceOffPixelsLeft > 160) tool.lookAheadDistanceOffPixelsLeft = 160;
    if (tool.lookAheadDistanceOffPixelsRight > 160) tool.lookAheadDistanceOffPixelsRight = 160;

    //determine if section is in boundary and headland using the section left/right positions
    bool isLeftIn = true, isRightIn = true;

    if (bnd.bndList.count() > 0)
    {
        for (int j = 0; j < tool.numOfSections; j++)
        {
            //only one first left point, the rest are all rights moved over to left
            isLeftIn = j == 0 ? bnd.IsPointInsideFenceArea(tool.section[j].leftPoint) : isRightIn;
            isRightIn = bnd.IsPointInsideFenceArea(tool.section[j].rightPoint);

            if (tool.isSectionOffWhenOut)
            {
                //merge the two sides into in or out
                if (isLeftIn || isRightIn) tool.section[j].isInBoundary = true;
                else tool.section[j].isInBoundary = false;
            }
            else
            {
                //merge the two sides into in or out
                if (!isLeftIn || !isRightIn) tool.section[j].isInBoundary = false;
                else tool.section[j].isInBoundary = true;
            }
        }
    }

    //determine farthest ahead lookahead - is the height of the readpixel line
    double rpHeight = 0;
    double rpOnHeight = 0;
    double rpToolHeight = 0;

    //pick the larger side
    if (CVehicle::instance()->hydLiftLookAheadDistanceLeft > CVehicle::instance()->hydLiftLookAheadDistanceRight) rpToolHeight = CVehicle::instance()->hydLiftLookAheadDistanceLeft;
    else rpToolHeight = CVehicle::instance()->hydLiftLookAheadDistanceRight;

    if (tool.lookAheadDistanceOnPixelsLeft > tool.lookAheadDistanceOnPixelsRight) rpOnHeight = tool.lookAheadDistanceOnPixelsLeft;
    else rpOnHeight = tool.lookAheadDistanceOnPixelsRight;

    isHeadlandClose = false;

    //clamp the height after looking way ahead, this is for switching off super section only
    rpOnHeight = fabs(rpOnHeight);
    rpToolHeight = fabs(rpToolHeight);

    //10 % min is required for overlap, otherwise it never would be on.
    int pixLimit = (int)((double)(tool.section[0].rpSectionWidth * rpOnHeight) / (double)(5.0));
    //bnd.isSectionControlledByHeadland = true;
    if ((rpOnHeight < rpToolHeight && this->isHeadlandOn() && bnd.isSectionControlledByHeadland)) rpHeight = rpToolHeight + 2;
    else rpHeight = rpOnHeight + 2;
    //qDebug(qpos) << bnd.isSectionControlledByHeadland << "headland sections";

    if (rpHeight > 290) rpHeight = 290;
    if (rpHeight < 8) rpHeight = 8;

    //read the whole block of pixels up to max lookahead, one read only
    //pixels are already read in another thread.

    //determine if headland is in read pixel buffer left middle and right.
    int start = 0, end = 0, tagged = 0, totalPixel = 0;

    //slope of the look ahead line
    double mOn = 0, mOff = 0;

    //tram and hydraulics
    if (tram.displayMode > 0 && tool.width > CVehicle::instance()->trackWidth)
    {
        tram.controlByte = 0;
        //1 pixels in is there a tram line?
        if (tram.isOuter)
        {
            if (grnPixels[(int)(tram.halfWheelTrack * 10)].green == 245) tram.controlByte += 2;
            if (grnPixels[tool.rpWidth - (int)(tram.halfWheelTrack * 10)].green == 245) tram.controlByte += 1;
        }
        else
        {
            if (grnPixels[tool.rpWidth / 2 - (int)(tram.halfWheelTrack * 10)].green == 245) tram.controlByte += 2;
            if (grnPixels[tool.rpWidth / 2 + (int)(tram.halfWheelTrack * 10)].green == 245) tram.controlByte += 1;
        }
    }
    else tram.controlByte = 0;

    //determine if in or out of headland, do hydraulics if on
    if (this->isHeadlandOn())
    {
        //calculate the slope
        double m = (CVehicle::instance()->hydLiftLookAheadDistanceRight - CVehicle::instance()->hydLiftLookAheadDistanceLeft) / tool.rpWidth;
        int height = 1;

        for (int pos = 0; pos < tool.rpWidth; pos++)
        {
            height = (int)(CVehicle::instance()->hydLiftLookAheadDistanceLeft + (m * pos)) - 1;
            for (int a = pos; a < height * tool.rpWidth; a += tool.rpWidth)
            {
                if (grnPixels[a].green == 250)
                {
                    isHeadlandClose = true;
                    goto GetOutTool;
                }
            }
        }

    GetOutTool: //goto

        //is the tool completely in the headland or not
        bnd.isToolInHeadland = bnd.isToolOuterPointsInHeadland && !isHeadlandClose;

        //set hydraulics based on tool in headland or not
        bnd.SetHydPosition(static_cast<btnStates>(this->autoBtnState()), p_239, *CVehicle::instance());

        //set hydraulics based on tool in headland or not
        bnd.SetHydPosition(static_cast<btnStates>(this->autoBtnState()), p_239, *CVehicle::instance());

    }

    ///////////////////////////////////////////   Section control        ssssssssssssssssssssss

    int endHeight = 1, startHeight = 1;

    if (this->isHeadlandOn() && bnd.isSectionControlledByHeadland) bnd.WhereAreToolLookOnPoints(*CVehicle::instance(), tool);

    for (int j = 0; j < tool.numOfSections; j++)
    {
        //Off or too slow or going backwards
        if (tool.sectionButtonState[j] == btnStates::Off || CVehicle::instance()->avgSpeed < CVehicle::instance()->slowSpeedCutoff || tool.section[j].speedPixels < 0)
        {
            tool.section[j].sectionOnRequest = false;
            tool.section[j].sectionOffRequest = true;

            // Manual on, force the section On
            if (tool.sectionButtonState[j] == btnStates::On)
            {
                tool.section[j].sectionOnRequest = true;
                tool.section[j].sectionOffRequest = false;
                continue;
            }
            continue;
        }

        // Manual on, force the section On
        if (tool.sectionButtonState[j] == btnStates::On)
        {
            tool.section[j].sectionOnRequest = true;
            tool.section[j].sectionOffRequest = false;
            continue;
        }


        //AutoSection - If any nowhere applied, send OnRequest, if its all green send an offRequest
        tool.section[j].isSectionRequiredOn = false;

        //calculate the slopes of the lines
        mOn = (tool.lookAheadDistanceOnPixelsRight - tool.lookAheadDistanceOnPixelsLeft) / tool.rpWidth;
        mOff = (tool.lookAheadDistanceOffPixelsRight - tool.lookAheadDistanceOffPixelsLeft) / tool.rpWidth;

        start = tool.section[j].rpSectionPosition - tool.section[0].rpSectionPosition;
        end = tool.section[j].rpSectionWidth - 1 + start;

        if (end >= tool.rpWidth)
            end = tool.rpWidth - 1;

        totalPixel = 1;
        tagged = 0;

        for (int pos = start; pos <= end; pos++)
        {
            startHeight = (int)(tool.lookAheadDistanceOffPixelsLeft + (mOff * pos)) * tool.rpWidth + pos;
            endHeight = (int)(tool.lookAheadDistanceOnPixelsLeft + (mOn * pos)) * tool.rpWidth + pos;

            for (int a = startHeight; a <= endHeight; a += tool.rpWidth)
            {
                totalPixel++;
                if (grnPixels[a].green == 0) tagged++;
            }
        }

        //determine if meeting minimum coverage
        tool.section[j].isSectionRequiredOn = ((tagged * 100) / totalPixel > (100 - tool.minCoverage));

        //logic if in or out of boundaries or headland
        if (bnd.bndList.count() > 0)
        {
            //if out of boundary, turn it off
            if (!tool.section[j].isInBoundary)
            {
                tool.section[j].isSectionRequiredOn = false;
                tool.section[j].sectionOffRequest = true;
                tool.section[j].sectionOnRequest = false;
                tool.section[j].sectionOffTimer = 0;
                tool.section[j].sectionOnTimer = 0;
                continue;
            }
            else
            {
                //is headland coming up
                if (this->isHeadlandOn() && bnd.isSectionControlledByHeadland)
                {
                    bool isHeadlandInLookOn = false;

                    //is headline in off to on area
                    mOn = (tool.lookAheadDistanceOnPixelsRight - tool.lookAheadDistanceOnPixelsLeft) / tool.rpWidth;
                    mOff = (tool.lookAheadDistanceOffPixelsRight - tool.lookAheadDistanceOffPixelsLeft) / tool.rpWidth;

                    start = tool.section[j].rpSectionPosition - tool.section[0].rpSectionPosition;

                    end = tool.section[j].rpSectionWidth - 1 + start;

                    if (end >= tool.rpWidth)
                        end = tool.rpWidth - 1;

                    tagged = 0;

                    for (int pos = start; pos <= end; pos++)
                    {
                        startHeight = (int)(tool.lookAheadDistanceOffPixelsLeft + (mOff * pos)) * tool.rpWidth + pos;
                        endHeight = (int)(tool.lookAheadDistanceOnPixelsLeft + (mOn * pos)) * tool.rpWidth + pos;

                        for (int a = startHeight; a <= endHeight; a += tool.rpWidth)
                        {
                            if (a < 0)
                                mOn = 0;
                            if (grnPixels[a].green == 250)
                            {
                                isHeadlandInLookOn = true;
                                goto GetOutHdOn;
                            }
                        }
                    }
                GetOutHdOn:

                    //determine if look ahead points are completely in headland
                    if (tool.section[j].isSectionRequiredOn && tool.section[j].isLookOnInHeadland && !isHeadlandInLookOn)
                    {
                        tool.section[j].isSectionRequiredOn = false;
                        tool.section[j].sectionOffRequest = true;
                        tool.section[j].sectionOnRequest = false;
                    }

                    if (tool.section[j].isSectionRequiredOn && !tool.section[j].isLookOnInHeadland && isHeadlandInLookOn)
                    {
                        tool.section[j].isSectionRequiredOn = true;
                        tool.section[j].sectionOffRequest = false;
                        tool.section[j].sectionOnRequest = true;
                    }
                }
            }
        }


        //global request to turn on section
        tool.section[j].sectionOnRequest = tool.section[j].isSectionRequiredOn;
        tool.section[j].sectionOffRequest = !tool.section[j].sectionOnRequest;

    }  // end of go thru all sections "for"

    //Set all the on and off times based from on off section requests
    for (int j = 0; j < tool.numOfSections; j++)
    {
        //SECTION timers

        if (tool.section[j].sectionOnRequest) {
            bool wasOn = tool.section[j].isSectionOn;
            tool.section[j].isSectionOn = true;
            // PHASE 6.0.36: sectionButtonState (user preference) should NOT be modified here
            // Only isSectionOn (calculated state) changes - matches C# original architecture
        }

        //turn off delay
        if (tool.turnOffDelay > 0)
        {
            if (!tool.section[j].sectionOffRequest) tool.section[j].sectionOffTimer = (int)(gpsHz / 2.0 * tool.turnOffDelay);

            if (tool.section[j].sectionOffTimer > 0) tool.section[j].sectionOffTimer--;

            if (tool.section[j].sectionOffRequest && tool.section[j].sectionOffTimer == 0)
            {
                if (tool.section[j].isSectionOn) {
                    tool.section[j].isSectionOn = false;
                    // PHASE 6.0.36: sectionButtonState (user preference) NOT modified
                    // Only isSectionOn (calculated state) changes - matches C# original
                }
            }
        }
        else
        {
            if (tool.section[j].sectionOffRequest) {
                bool wasOn = tool.section[j].isSectionOn;
                tool.section[j].isSectionOn = false;
                // PHASE 6.0.36: sectionButtonState (user preference) NOT modified here
                // Only isSectionOn (calculated state) changes - matches C# original architecture
                // sectionButtonState controlled ONLY by user actions: button clicks, Master Auto
            }
        }

        //Mapping timers
        if (tool.section[j].sectionOnRequest && !tool.section[j].isMappingOn && tool.section[j].mappingOnTimer == 0)
        {
            tool.section[j].mappingOnTimer = (int)(tool.lookAheadOnSetting * (gpsHz / 2) - 1);
        }
        else if (tool.section[j].sectionOnRequest && tool.section[j].isMappingOn && tool.section[j].mappingOffTimer > 1)
        {
            tool.section[j].mappingOffTimer = 0;
            tool.section[j].mappingOnTimer = (int)(tool.lookAheadOnSetting * (gpsHz / 2) - 1);
        }

        if (tool.lookAheadOffSetting > 0)
        {
            if (tool.section[j].sectionOffRequest && tool.section[j].isMappingOn && tool.section[j].mappingOffTimer == 0)
            {
                tool.section[j].mappingOffTimer = (int)(tool.lookAheadOffSetting * (gpsHz / 2) + 4);
            }
        }
        else if (tool.turnOffDelay > 0)
        {
            if (tool.section[j].sectionOffRequest && tool.section[j].isMappingOn && tool.section[j].mappingOffTimer == 0)
                tool.section[j].mappingOffTimer = (int)(tool.turnOffDelay * gpsHz / 2);
        }
        else
        {
            tool.section[j].mappingOffTimer = 0;
        }

        //MAPPING - Not the making of triangle patches - only status - on or off
        if (tool.section[j].sectionOnRequest)
        {
            tool.section[j].mappingOffTimer = 0;
            if (tool.section[j].mappingOnTimer > 1)
                tool.section[j].mappingOnTimer--;
            else
            {
                tool.section[j].isMappingOn = true;
            }
        }

        if (tool.section[j].sectionOffRequest)
        {
            tool.section[j].mappingOnTimer = 0;
            if (tool.section[j].mappingOffTimer > 1)
                tool.section[j].mappingOffTimer--;
            else
            {
                tool.section[j].isMappingOn = false;
            }
        }
    }

    //Checks the workswitch or steerSwitch if required
    if (ahrs.isAutoSteerAuto || mc.isRemoteWorkSystemOn)
        mc.CheckWorkAndSteerSwitch(ahrs,isBtnAutoSteerOn());

    // check if any sections have changed status
    number = 0;

    for (int j = 0; j < tool.numOfSections; j++)
    {
        if (tool.section[j].isMappingOn)
        {
            number |= 1ul << j;
        }
    }

    //there has been a status change of section on/off
    if (number != lastNumber)
    {
        int sectionOnOffZones = 0, patchingZones = 0;

        //everything off
        if (number == 0)
        {
            for (int j = 0; j < triStrip.count(); j++)
            {
                if (triStrip[j].isDrawing)
                    triStrip[j].TurnMappingOff(tool, fd, mainWindow, this);
            }
        }
        else if (!tool.isMultiColoredSections)
        {
            //set the start and end positions from section points
            for (int j = 0; j < tool.numOfSections; j++)
            {
                //skip till first mapping section
                if (!tool.section[j].isMappingOn) continue;

                //do we need more patches created
                if (triStrip.count() < sectionOnOffZones + 1)
                    triStrip.append(CPatches());

                //set this strip start edge to edge of this section
                triStrip[sectionOnOffZones].newStartSectionNum = j;

                while ((j + 1) < tool.numOfSections && tool.section[j + 1].isMappingOn)
                {
                    j++;
                }

                //set the edge of this section to be end edge of strp
                triStrip[sectionOnOffZones].newEndSectionNum = j;
                sectionOnOffZones++;
            }

            //count current patch strips being made
            for (int j = 0; j < triStrip.count(); j++)
            {
                if (triStrip[j].isDrawing) patchingZones++;
            }

            //tests for creating new strips or continuing
            bool isOk = (patchingZones == sectionOnOffZones && sectionOnOffZones < 3);

            if (isOk)
            {
                for (int j = 0; j < sectionOnOffZones; j++)
                {
                    if (triStrip[j].newStartSectionNum > triStrip[j].currentEndSectionNum
                        || triStrip[j].newEndSectionNum < triStrip[j].currentStartSectionNum)
                        isOk = false;
                }
            }

            if (isOk)
            {
                for (int j = 0; j < sectionOnOffZones; j++)
                {
                    if (triStrip[j].newStartSectionNum != triStrip[j].currentStartSectionNum
                        || triStrip[j].newEndSectionNum != triStrip[j].currentEndSectionNum)
                    {
                        //if (tool.isSectionsNotZones)
                        {
                            triStrip[j].AddMappingPoint(tool,fd, 0, mainWindow, this);
                        }

                        triStrip[j].currentStartSectionNum = triStrip[j].newStartSectionNum;
                        triStrip[j].currentEndSectionNum = triStrip[j].newEndSectionNum;
                        triStrip[j].AddMappingPoint(tool,fd, 0, mainWindow, this);
                    }
                }
            }
            else
            {
                //too complicated, just make new strips
                for (int j = 0; j < triStrip.count(); j++)
                {
                    if (triStrip[j].isDrawing)
                        triStrip[j].TurnMappingOff(tool, fd, mainWindow, this);
                }

                for (int j = 0; j < sectionOnOffZones; j++)
                {
                    triStrip[j].currentStartSectionNum = triStrip[j].newStartSectionNum;
                    triStrip[j].currentEndSectionNum = triStrip[j].newEndSectionNum;
                    triStrip[j].TurnMappingOn(tool, 0);
                }
            }
        }
        else if (tool.isMultiColoredSections) //could be else only but this is more clear
        {
            //set the start and end positions from section points
            for (int j = 0; j < tool.numOfSections; j++)
            {
                //do we need more patches created
                if (triStrip.count() < sectionOnOffZones + 1)
                    triStrip.append(CPatches());

                //set this strip start edge to edge of this section
                triStrip[sectionOnOffZones].newStartSectionNum = j;

                //set the edge of this section to be end edge of strp
                triStrip[sectionOnOffZones].newEndSectionNum = j;
                sectionOnOffZones++;

                if (!tool.section[j].isMappingOn)
                {
                    if (triStrip[j].isDrawing)
                        triStrip[j].TurnMappingOff(tool, fd, mainWindow, this);
                }
                else
                {
                    triStrip[j].currentStartSectionNum = triStrip[j].newStartSectionNum;
                    triStrip[j].currentEndSectionNum = triStrip[j].newEndSectionNum;
                    triStrip[j].TurnMappingOn(tool,j);
                }
            }
        }


        lastNumber = number;
    }

    //send the byte out to section machines
    BuildMachineByte();

    //if a minute has elapsed save the field in case of crash and to be able to resume
    if (minuteCounter > 30 && this->sentenceCounter() < 20)
    {
        // Phase 2.4: No longer need to stop timer - saves are now fast (< 50ms)
        // tmrWatchdog->stop();  // REMOVED - buffered saves don't block GPS

        //don't save if no gps
        if (isJobStarted())
        {
            //auto save the field patches, contours accumulated so far
            FileSaveSections();  // Now < 50ms with buffering
            FileSaveContour();   // Now < 50ms with buffering

            //NMEA log file
            //TODO: if (isLogElevation) FileSaveElevation(;
            //ExportFieldAs_KML(;
        }

        //if its the next day, calc sunrise sunset for next day
        minuteCounter = 0;

        //set saving flag off
        isSavingFile = false;

        // Phase 2.4: No longer need to restart timer
        // tmrWatchdog->start();  // REMOVED - timer never stopped

        //calc overlap
        //oglZoom.Refresh(;

    }

    if (isJobStarted())
    {
        p_239.pgn[p_239.geoStop] = this->isOutOfBounds() ? 1 : 0;

        // SendPgnToLoop(p_239.pgn;  // âŒ REMOVED - Phase 4.6: AgIOService Workers handle PGN

        // SendPgnToLoop(p_229.pgn;  // âŒ REMOVED - Phase 4.6: Use AgIOService.sendPgn() instead
        if (m_agioService) {
            m_agioService->sendPgn(p_239.pgn);
            m_agioService->sendPgn(p_229.pgn);
        }
    }

#ifdef USE_QPAINTER_BACKBUFFER
    qWarning() << "After threaded back buffer drawing, section lookahead finished at " << swFrame.elapsed();
    }, Qt::QueuedConnection);
    });
#endif

    //lock.unlock(;

    //this is the end of the "frame". Now we wait for next NMEA sentence with a valid fix.
}


void FormGPS::CalculatePositionHeading()
{
    // #region pivot hitch trail
    //Probably move this into CVehicle

    //translate from pivot position to steer axle and pivot axle position
    //translate world to the pivot axle
    CVehicle::instance()->pivotAxlePos.easting = pn.fix.easting - (sin(CVehicle::instance()->fixHeading) * CVehicle::instance()->antennaPivot);
    CVehicle::instance()->pivotAxlePos.northing = pn.fix.northing - (cos(CVehicle::instance()->fixHeading) * CVehicle::instance()->antennaPivot);
    CVehicle::instance()->pivotAxlePos.heading = CVehicle::instance()->fixHeading;

    CVehicle::instance()->steerAxlePos.easting = CVehicle::instance()->pivotAxlePos.easting + (sin(CVehicle::instance()->fixHeading) * CVehicle::instance()->wheelbase);
    CVehicle::instance()->steerAxlePos.northing = CVehicle::instance()->pivotAxlePos.northing + (cos(CVehicle::instance()->fixHeading) * CVehicle::instance()->wheelbase);
    CVehicle::instance()->steerAxlePos.heading = CVehicle::instance()->fixHeading;
    
    // PHASE 4.3: Measure execution latency for vehicle position update
    // This measures the TIME BETWEEN calls (which gives us frequency)
    static QElapsedTimer intervalTimer;
    static bool intervalTimerStarted = false;
    if (!intervalTimerStarted) {
        intervalTimer.start();
        intervalTimerStarted = true;
    }
    
    // Measure interval between calls (for frequency calculation)
    qint64 intervalBetweenCalls = intervalTimer.nsecsElapsed();
    intervalTimer.restart();
    
    // For actual execution latency, we need a different approach
    // The execution of this function should be < 1ms
    // The interval between calls is ~100ms at 10Hz which is normal for GPS
    
    static int latencyLogCounter = 0;
    if (++latencyLogCounter % 500 == 0) { // Log every 500 updates (~10s at 50Hz actual frequency)
        double actualHz = 1000000000.0 / intervalBetweenCalls; // Convert ns to Hz
        qDebug(qpos) << "📊 UpdateFixPosition - Interval:" << intervalBetweenCalls/1000000 << "ms"
                 << "Actual Hz:" << actualHz << "GPS Hz:" << gpsHz;
    }

    //guidance look ahead distance based on time or tool width at least

    if (!track.ABLine.isLateralTriggered && !track.curve.isLateralTriggered)
    {
        double guidanceLookDist = (max(tool.width * 0.5, CVehicle::instance()->avgSpeed * 0.277777 * guidanceLookAheadTime));
        CVehicle::instance()->guidanceLookPos.easting = CVehicle::instance()->pivotAxlePos.easting + (sin(CVehicle::instance()->fixHeading) * guidanceLookDist);
        CVehicle::instance()->guidanceLookPos.northing = CVehicle::instance()->pivotAxlePos.northing + (cos(CVehicle::instance()->fixHeading) * guidanceLookDist);
    }

    //determine where the rigid vehicle hitch ends
    CVehicle::instance()->hitchPos.easting = pn.fix.easting + (sin(CVehicle::instance()->fixHeading) * (tool.hitchLength - CVehicle::instance()->antennaPivot));
    CVehicle::instance()->hitchPos.northing = pn.fix.northing + (cos(CVehicle::instance()->fixHeading) * (tool.hitchLength - CVehicle::instance()->antennaPivot));

    //tool attached via a trailing hitch
    if (tool.isToolTrailing)
    {
        double over;
        if (tool.isToolTBT)
        {
            //Torriem rules!!!!! Oh yes, this is all his. Thank-you
            if (distanceCurrentStepFix != 0)
            {
                CVehicle::instance()->tankPos.heading = atan2(CVehicle::instance()->hitchPos.easting - CVehicle::instance()->tankPos.easting, CVehicle::instance()->hitchPos.northing - CVehicle::instance()->tankPos.northing);
                if (CVehicle::instance()->tankPos.heading < 0) CVehicle::instance()->tankPos.heading += glm::twoPI;
            }

            ////the tool is seriously jacknifed or just starting out so just spring it back.
            over = fabs(M_PI - fabs(fabs(CVehicle::instance()->tankPos.heading - CVehicle::instance()->fixHeading) - M_PI));

            if ((over < 2.0) && (startCounter > 50))
            {
                CVehicle::instance()->tankPos.easting = CVehicle::instance()->hitchPos.easting + (sin(CVehicle::instance()->tankPos.heading) * (tool.tankTrailingHitchLength));
                CVehicle::instance()->tankPos.northing = CVehicle::instance()->hitchPos.northing + (cos(CVehicle::instance()->tankPos.heading) * (tool.tankTrailingHitchLength));
            }

            //criteria for a forced reset to put tool directly behind vehicle
            if (over > 2.0 || startCounter < 51 )
            {
                CVehicle::instance()->tankPos.heading = CVehicle::instance()->fixHeading;
                CVehicle::instance()->tankPos.easting = CVehicle::instance()->hitchPos.easting + (sin(CVehicle::instance()->tankPos.heading) * (tool.tankTrailingHitchLength));
                CVehicle::instance()->tankPos.northing = CVehicle::instance()->hitchPos.northing + (cos(CVehicle::instance()->tankPos.heading) * (tool.tankTrailingHitchLength));
            }

        }

        else
        {
            CVehicle::instance()->tankPos.heading = CVehicle::instance()->fixHeading;
            CVehicle::instance()->tankPos.easting = CVehicle::instance()->hitchPos.easting;
            CVehicle::instance()->tankPos.northing = CVehicle::instance()->hitchPos.northing;
        }

        //Torriem rules!!!!! Oh yes, this is all his. Thank-you
        if (distanceCurrentStepFix != 0)
        {
            CVehicle::instance()->toolPivotPos.heading = atan2(CVehicle::instance()->tankPos.easting - CVehicle::instance()->toolPivotPos.easting, CVehicle::instance()->tankPos.northing - CVehicle::instance()->toolPivotPos.northing);
            if (CVehicle::instance()->toolPivotPos.heading < 0) CVehicle::instance()->toolPivotPos.heading += glm::twoPI;
        }

        ////the tool is seriously jacknifed or just starting out so just spring it back.
        over = fabs(M_PI - fabs(fabs(CVehicle::instance()->toolPivotPos.heading - CVehicle::instance()->tankPos.heading) - M_PI));

        if ((over < 1.9) && (startCounter > 50))
        {
            CVehicle::instance()->toolPivotPos.easting = CVehicle::instance()->tankPos.easting + (sin(CVehicle::instance()->toolPivotPos.heading) * (tool.trailingHitchLength));
            CVehicle::instance()->toolPivotPos.northing = CVehicle::instance()->tankPos.northing + (cos(CVehicle::instance()->toolPivotPos.heading) * (tool.trailingHitchLength));
        }

        //criteria for a forced reset to put tool directly behind vehicle
        if (over > 1.9 || startCounter < 51 )
        {
            CVehicle::instance()->toolPivotPos.heading = CVehicle::instance()->tankPos.heading;
            CVehicle::instance()->toolPivotPos.easting = CVehicle::instance()->tankPos.easting + (sin(CVehicle::instance()->toolPivotPos.heading) * (tool.trailingHitchLength));
            CVehicle::instance()->toolPivotPos.northing = CVehicle::instance()->tankPos.northing + (cos(CVehicle::instance()->toolPivotPos.heading) * (tool.trailingHitchLength));
        }

        CVehicle::instance()->toolPos.heading = CVehicle::instance()->toolPivotPos.heading;
        CVehicle::instance()->toolPos.easting = CVehicle::instance()->tankPos.easting +
                                  (sin(CVehicle::instance()->toolPivotPos.heading) * (tool.trailingHitchLength - tool.trailingToolToPivotLength));
        CVehicle::instance()->toolPos.northing = CVehicle::instance()->tankPos.northing +
                                   (cos(CVehicle::instance()->toolPivotPos.heading) * (tool.trailingHitchLength - tool.trailingToolToPivotLength));

    }

    //rigidly connected to vehicle
    else
    {
        CVehicle::instance()->toolPivotPos.heading = CVehicle::instance()->fixHeading;
        CVehicle::instance()->toolPivotPos.easting = CVehicle::instance()->hitchPos.easting;
        CVehicle::instance()->toolPivotPos.northing = CVehicle::instance()->hitchPos.northing;

        CVehicle::instance()->toolPos.heading = CVehicle::instance()->fixHeading;
        CVehicle::instance()->toolPos.easting = CVehicle::instance()->hitchPos.easting;
        CVehicle::instance()->toolPos.northing = CVehicle::instance()->hitchPos.northing;
    }

    //#endregion

    //used to increase triangle count when going around corners, less on straight
    //pick the slow moving side edge of tool
    double distance = tool.width * 0.5;
    if (distance > 5) distance = 5;

    //whichever is less
    if (tool.farLeftSpeed < tool.farRightSpeed)
    {
        double twist = tool.farLeftSpeed / tool.farRightSpeed;
        twist *= twist;
        if (twist < 0.2) twist = 0.2;
        CVehicle::instance()->sectionTriggerStepDistance = distance * twist * twist;
    }
    else
    {
        double twist = tool.farRightSpeed / tool.farLeftSpeed;
        //twist *= twist;
        if (twist < 0.2) twist = 0.2;

        CVehicle::instance()->sectionTriggerStepDistance = distance * twist * twist;
    }

    //finally fixed distance for making a curve line
    if (!track.curve.isMakingCurve) CVehicle::instance()->sectionTriggerStepDistance = CVehicle::instance()->sectionTriggerStepDistance + 0.5;
    //if (this->isContourBtnOn()) CVehicle::instance()->sectionTriggerStepDistance *=0.5;

    //precalc the sin and cos of heading * -1
    CVehicle::instance()->sinSectionHeading = sin(-CVehicle::instance()->toolPivotPos.heading);
    CVehicle::instance()->cosSectionHeading = cos(-CVehicle::instance()->toolPivotPos.heading);
}

//calculate the extreme tool left, right velocities, each section lookahead, and whether or not its going backwards
void FormGPS::CalculateSectionLookAhead(double northing, double easting, double cosHeading, double sinHeading)
{
    //calculate left side of section 1
    Vec2 left;
    Vec2 right = left;
    double leftSpeed = 0, rightSpeed = 0;

    //speed max for section kmh*0.277 to m/s * 10 cm per pixel * 1.7 max speed
    double meterPerSecPerPixel = fabs(CVehicle::instance()->avgSpeed) * 4.5;
    //qDebug(qpos) << pn.speed << ", m/s per pixel is " << meterPerSecPerPixel;

    //now loop all the section rights and the one extreme left
    for (int j = 0; j < tool.numOfSections; j++)
    {
        if (j == 0)
        {
            //only one first left point, the rest are all rights moved over to left
            tool.section[j].leftPoint = Vec2(cosHeading * (tool.section[j].positionLeft) + easting,
                                             sinHeading * (tool.section[j].positionLeft) + northing);

            left = tool.section[j].leftPoint - tool.section[j].lastLeftPoint;

            //save a copy for next time
            tool.section[j].lastLeftPoint = tool.section[j].leftPoint;

            //get the speed for left side only once

            leftSpeed = left.getLength() * gpsHz * 10;
            //qDebug(qpos) << leftSpeed << " - left speed";
            if (leftSpeed > meterPerSecPerPixel) leftSpeed = meterPerSecPerPixel;
        }
        else
        {
            //right point from last section becomes this left one
            tool.section[j].leftPoint = tool.section[j - 1].rightPoint;
            left = tool.section[j].leftPoint - tool.section[j].lastLeftPoint;

            //save a copy for next time
            tool.section[j].lastLeftPoint = tool.section[j].leftPoint;

            //save the slower of the 2
            if (leftSpeed > rightSpeed) leftSpeed = rightSpeed;
        }

        tool.section[j].rightPoint = Vec2(cosHeading * (tool.section[j].positionRight) + easting,
                                          sinHeading * (tool.section[j].positionRight) + northing);
        /*
        qDebug(qpos) << j << ": " << tool.section[j].leftPoint.easting << "," <<
                                 tool.section[j].leftPoint.northing <<" " <<
                                 tool.section[j].rightPoint.easting << ", " <<
                                 tool.section[j].rightPoint.northing;
                                 */


        //now we have left and right for this section
        right = tool.section[j].rightPoint - tool.section[j].lastRightPoint;

        //save a copy for next time
        tool.section[j].lastRightPoint = tool.section[j].rightPoint;

        //grab vector length and convert to meters/sec/10 pixels per meter
        rightSpeed = right.getLength() * gpsHz * 10;
        if (rightSpeed > meterPerSecPerPixel) rightSpeed = meterPerSecPerPixel;

        //Is section outer going forward or backward
        double head = left.headingXZ();

        if (head < 0) head += glm::twoPI;

        if (M_PI - fabs(fabs(head - CVehicle::instance()->toolPos.heading) - M_PI) > glm::PIBy2)
        {
            if (leftSpeed > 0) leftSpeed *= -1;
        }

        head = right.headingXZ();
        if (head < 0) head += glm::twoPI;
        if (M_PI - fabs(fabs(head - CVehicle::instance()->toolPos.heading) - M_PI) > glm::PIBy2)
        {
            if (rightSpeed > 0) rightSpeed *= -1;
        }

        double sped = 0;
        //save the far left and right speed in m/sec averaged over 20%
        if (j==0)
        {
            sped = (leftSpeed * 0.1);
            if (sped < 0.1) sped = 0.1;
            tool.farLeftSpeed = tool.farLeftSpeed * 0.7 + sped * 0.3;
            //qWarning() << sped << tool.farLeftSpeed << CVehicle::instance()->avgSpeed;
        }

        if (j == tool.numOfSections - 1)
        {
            sped = (rightSpeed * 0.1);
            if(sped < 0.1) sped = 0.1;
            tool.farRightSpeed = tool.farRightSpeed * 0.7 + sped * 0.3;
        }
        //choose fastest speed
        if (leftSpeed > rightSpeed)
        {
            sped = leftSpeed;
            leftSpeed = rightSpeed;
        }
        else sped = rightSpeed;
        tool.section[j].speedPixels = tool.section[j].speedPixels * 0.7 + sped * 0.3;
    }
}

//perimeter and boundary point generation
void FormGPS::AddBoundaryPoint()
{
    //save the north & east as previous
    prevBoundaryPos.easting = pn.fix.easting;
    prevBoundaryPos.northing = pn.fix.northing;

    //build the boundary line
    if (bnd.isOkToAddPoints)
    {
        if (this->isDrawRightSide())
        {
            //Right side
            Vec3 point(CVehicle::instance()->pivotAxlePos.easting + sin(CVehicle::instance()->pivotAxlePos.heading - glm::PIBy2) * -this->createBndOffset(),
                       CVehicle::instance()->pivotAxlePos.northing + cos(CVehicle::instance()->pivotAxlePos.heading - glm::PIBy2) * -this->createBndOffset(),
                       CVehicle::instance()->pivotAxlePos.heading);
            bnd.bndBeingMadePts.append(point);
        }

        //draw on left side
        else
        {
            //Right side
            Vec3 point(CVehicle::instance()->pivotAxlePos.easting + sin(CVehicle::instance()->pivotAxlePos.heading - glm::PIBy2) * this->createBndOffset(),
                       CVehicle::instance()->pivotAxlePos.northing + cos(CVehicle::instance()->pivotAxlePos.heading - glm::PIBy2) * this->createBndOffset(),
                       CVehicle::instance()->pivotAxlePos.heading);
            bnd.bndBeingMadePts.append(point);
        }
        boundary_calculate_area(); //in formgps_ui_boundary.cpp
    }
}

void FormGPS::AddContourPoints()
{
    //if (isConstantContourOn)
    {
        //record contour all the time
        //Contour Base Track.... At least One section on, turn on if not
        if (patchCounter != 0)
        {
            //keep the line going, everything is on for recording path
            if (ct.isContourOn) ct.AddPoint(CVehicle::instance()->pivotAxlePos);
            else
            {
                ct.StartContourLine();
                ct.AddPoint(CVehicle::instance()->pivotAxlePos);
            }
        }

        //All sections OFF so if on, turn off
        else
        {
            if (ct.isContourOn)
            { ct.StopContourLine(contourSaveList); }
        }

        //Build contour line if close enough to a patch
        if (this->isContourBtnOn()) ct.BuildContourGuidanceLine(secondsSinceStart, *CVehicle::instance(), CVehicle::instance()->pivotAxlePos, mainWindow);
    }
    //save the north & east as previous
    prevContourPos.northing = CVehicle::instance()->pivotAxlePos.northing;
    prevContourPos.easting = CVehicle::instance()->pivotAxlePos.easting;
}

//add the points for section, contour line points, Area Calc feature
void FormGPS::AddSectionOrPathPoints()
{
    if (recPath.isRecordOn)
    {
        //keep minimum speed of 1.0
        double speed = CVehicle::instance()->avgSpeed;
        if (CVehicle::instance()->avgSpeed < 1.0) speed = 1.0;
        bool autoBtn = (this->autoBtnState() == btnStates::Auto);

        recPath.recList.append(CRecPathPt(CVehicle::instance()->pivotAxlePos.easting, CVehicle::instance()->pivotAxlePos.northing, CVehicle::instance()->pivotAxlePos.heading, speed, autoBtn));
    }

    track.AddPathPoint(CVehicle::instance()->pivotAxlePos);

    //save the north & east as previous
    prevSectionPos.northing = pn.fix.northing;
    prevSectionPos.easting = pn.fix.easting;

    // if non zero, at least one section is on.
    patchCounter = 0;

    //send the current and previous GPS fore/aft corrected fix to each section
    for (int j = 0; j < triStrip.count(); j++)
    {
        if (triStrip[j].isDrawing)
        {
            if (this->isPatchesChangingColor())
            {
                triStrip[j].numTriangles = 64;
                this->setIsPatchesChangingColor(false);
            }

            triStrip[j].AddMappingPoint(tool, fd, j, mainWindow, this);
            patchCounter++;
        }
    }
}

//the start of first few frames to initialize entire program
void FormGPS::InitializeFirstFewGPSPositions()
{
    if (!isFirstFixPositionSet)
    {
        // PHASE 6.0.41: Force latStart/lonStart update when switching modes, even if field open
        // Prevents gray screen when GPS arrives after SIM->REAL switch with open field
        if (!isJobStarted() || m_forceGPSReinitialization)
        {
            // PHASE 6.0.42.5: Validate GPS coordinates before initialization
            // Race condition fix: Timer (40 Hz) can trigger BEFORE GPS data arrives after mode switch
            // Scenario: SIM→REAL→UDP ON → timer tick at T+25ms, GPS arrives at T+50-200ms
            // If pn.latitude/longitude == 0 → wait for next cycle (25ms) instead of corrupting latStart/lonStart
            // Prevents initializing coordinate reference with invalid (0,0) which causes gray screen
            if (pn.latitude == 0 || pn.longitude == 0) {
                // Invalid coordinates - wait for real GPS data
                // Do NOT clear m_forceGPSReinitialization flag
                // Do NOT set isFirstFixPositionSet = true
                return;  // Retry next cycle (25ms later)
            }

            // Valid coordinates → initialize normally
            // Phase 6.3.1: Use PropertyWrapper for safe property access
            this->setLatStart(pn.latitude);
            // Phase 6.3.1: Use PropertyWrapper for safe property access
            this->setLonStart(pn.longitude);
            // Phase 6.3.1: Use PropertyWrapper for safe QObject access
            pn.SetLocalMetersPerDegree(this);

            // PHASE 6.0.41: Clear flag after successful reinitialization
            if (m_forceGPSReinitialization) {
                m_forceGPSReinitialization = false;
            }
        }

        // Phase 6.3.1: Use PropertyWrapper for safe QObject access
        pn.ConvertWGS84ToLocal(pn.latitude, pn.longitude, pn.fix.northing, pn.fix.easting, this);

        //Draw a grid once we know where in the world we are.
        isFirstFixPositionSet = true;

        //most recent fixes
        prevFix.easting = pn.fix.easting;
        prevFix.northing = pn.fix.northing;

        //run once and return
        isFirstFixPositionSet = true;

        return;
    }

    else
    {

        //most recent fixes
        prevFix.easting = pn.fix.easting; prevFix.northing = pn.fix.northing;

        //keep here till valid data
        if (startCounter > (20))
        {
            isGPSPositionInitialized = true;
            lastReverseFix = pn.fix;
        }

        //in radians
        CVehicle::instance()->fixHeading = 0;
        CVehicle::instance()->toolPos.heading = CVehicle::instance()->fixHeading;

        //send out initial zero settings
        if (isGPSPositionInitialized)
        {
            //TODO determine if it is day from wall clock and date
            isDayTime = true;

            camera.SetZoom();
        }
        return;
    }
}

// Phase 6.0.21: Receive parsed data from AgIOService broadcast signal
void FormGPS::onParsedDataReady(const PGNParser::ParsedData& data)
{
    if (!data.isValid) return;

    // Phase 6.0.21.12: Ignore UDP GPS data when simulation is ON
    // Prevents conflict: simulation data vs real UDP data fighting for same Q_PROPERTY
    // When simulation ON + AgIO ON + UDP ON → simulation has priority, ignore UDP
    if (SettingsManager::instance()->menu_isSimulatorOn()) {
        return;  // Simulation mode active - ignore real GPS data from UDP
    }

    // ===== Phase 6.0.23.4: Update INTERNAL structures at 40 Hz (real-time calculations) =====
    // Phase 6.0.21.11: Update pn structure before UpdateFixPosition() (like simulation mode)
    // CRITICAL: UpdateFixPosition() line 1246 checks "if (m_latitude != pn.latitude)"
    // If pn.latitude is not updated, it overwrites m_latitude with stale value (0) → binding conflict

    // ✅ Problem 14 Fix (Enhanced): Validate GPS FIX QUALITY before assigning ANY data
    // NMEA Standard:
    // - quality=0 → "Invalid" (no GPS fix)
    // - satellites=0 → No satellites tracked (impossible to have position)
    // Strategy: Reject entire fix if invalid, don't overwrite last valid position

    // Always update quality/satellites (metadata about GPS state)
    pn.fixQuality = data.quality;
    pn.satellitesTracked = data.satellites;

    // Check if GPS fix is VALID (quality > 0 AND satellites > 0)
    bool validGpsFix = (data.quality > 0 && data.satellites > 0);

    if (validGpsFix) {
        // GPS fix valid → update coordinates IF non-zero (empty NMEA field protection)
        if (data.latitude != 0.0 && data.longitude != 0.0) {
            // PHASE 6.0.42: Check for GPS jump before updating position
            // Handles SIM→REAL mode switch, GPS module change, position corrections
            // If jump detected: closes field (if open), updates latStart/lonStart, resets flags
            if (detectGPSJump(data.latitude, data.longitude)) {
                handleGPSJump(data.latitude, data.longitude);
            }

            pn.latitude = data.latitude;
            pn.longitude = data.longitude;
        }

        // Update other GPS data (only if fix valid)
        // Note: altitude=0 is VALID (sea level), age=0 is VALID (no differential correction)
        pn.altitude = data.altitude;
        pn.hdop = data.hdop;
        pn.age = data.age;
    } else {
        // GPS fix INVALID (quality=0 or satellites=0)
        // Don't update position/altitude/hdop/age → preserve last valid values
        // This prevents display from jumping to 0 when GPS signal lost momentarily
        qDebug(qpos) << "❌ GPS fix INVALID - quality:" << data.quality
                 << "satellites:" << data.satellites
                 << "→ preserving last valid position";
    }

    // Update heading in pn structure
    if (data.headingDual > 0) {
        pn.headingTrue = pn.headingTrueDual = data.headingDual;
    } else if (data.heading > 0) {
        pn.headingTrue = data.heading;
        pn.headingTrueDual = 0;  // No dual antenna
    }

    // Speed
    pn.vtgSpeed = data.speed;

    // ✅ CRITICAL FIX: Convert lat/lon to northing/easting at EVERY packet (like simulation)
    // BUG: Without this, northing/easting were only calculated ONCE (first position)
    // → Lat/lon updated every packet but northing/easting stayed at old values
    // → Roll corrections applied to stale northing/easting → tracteur pivote!
    // Simulation does this correctly (formgps_sim.cpp:55) - real mode must match
    if (validGpsFix) {
        pn.ConvertWGS84ToLocal(pn.latitude, pn.longitude, pn.fix.northing, pn.fix.easting, this);
    }

    // ✅ Problem 14 Fix (Final): Store IMU data in ahrs structure (40 Hz)
    // Update ahrs structure every time IMU data arrives
    if (data.hasIMU) {
        // Store in ahrs structure (used by calculations)
        ahrs.imuHeading = data.imuHeading;
        ahrs.imuRoll = data.imuRoll;
        ahrs.imuPitch = data.imuPitch;
        ahrs.imuYawRate = data.yawRate;

        // ✅ NO THROTTLING: Assign directly to Q_PROPERTY (40 Hz)
        // Qt optimizes: only triggers QML update if value actually changed
        // Simpler architecture: no intermediate storage, no sync bugs
        setImuHeading(data.imuHeading);  // 0° = north (VALID)
        setImuRoll(data.imuRoll);        // 0° = horizontal (VALID)
        setImuPitch(data.imuPitch);      // 0° = no slope (VALID)
        setYawRate(data.yawRate);        // 0°/s = no rotation (VALID)
    }

    // PHASE 6.0.23: Store AutoSteer control data if present (PGN 253/250) - 40 Hz
    if (data.hasSteerData) {
        // Steer Angle Actual (from PGN 253 byte 5-6)
        if (data.steerAngleActual != 0) {
            mc.actualSteerAngleDegrees = data.steerAngleActual * 0.01;
        }

        // Switch Status (from PGN 253 byte 11)
        if (data.switchByte != 0) {
            mc.workSwitchHigh = (data.switchByte & 0x01) == 0x01;
            mc.steerSwitchHigh = (data.switchByte & 0x02) == 0x02;
            mc.CheckWorkAndSteerSwitch(ahrs, isBtnAutoSteerOn());
        }

        // PWM Display (from PGN 253 byte 12)
        if (data.pwmDisplay != 0) {
            mc.pwmDisplay = data.pwmDisplay;
        }

        // Sensor Value (from PGN 250 byte 5)
        if (data.sensorValue != 0) {
            mc.sensorData = data.sensorValue;
        }
    }

    // ✅ PHASE 6.0.21.9: Reset sentenceCounter on valid NMEA data (prevents "No GPS" false alarm)
    this->setSentenceCounter(0);

    // Phase 6.0.24: UpdateFixPosition() moved to timerGPS callback (40 Hz fixed rate)
    // onParsedDataReady() now ONLY stores data in pn/ahrs structures
    // timerGPS (25ms = 40 Hz) calls onGPSTimerTimeout() → UpdateFixPosition()
    // This prevents UpdateFixPosition() from being called at UDP packet rate (700 Hz bug!)
    // Architecture now matches simulation mode: data storage separated from position update

    // ===== Phase 6.0.23.4: Update Q_PROPERTY for QML at 10 Hz (display only) =====
    static int qmlUpdateCounter = 0;
    if (++qmlUpdateCounter % 4 == 0) {  // 40 Hz / 4 = 10 Hz for QML display
        // Position data (10 Hz → QML)
        // ✅ Problem 14 Fix: pn.latitude/longitude now validated above (not overwritten with 0)
        // Safe to assign - will use last valid position if current data was 0
        setLatitude(pn.latitude);
        setLongitude(pn.longitude);
        setAltitude(pn.altitude);

        // Heading (10 Hz → QML)
        if (data.headingDual > 0) {
            setHeading(data.headingDual);
        } else if (data.heading > 0) {
            setHeading(data.heading);
        }

        // Speed (10 Hz → QML)
        setSpeedKph(pn.vtgSpeed);

        // GPS quality indicators (10 Hz → QML)
        setHdop(pn.hdop);
        setAge(pn.age);
        setFixQuality(pn.fixQuality);
        setSatellitesTracked(pn.satellitesTracked);

        // ✅ IMU data is now assigned directly at 40 Hz (see above, no throttling)
        // Simpler: no need to duplicate assignment here

        // AutoSteer display data (10 Hz → QML)
        if (data.hasSteerData) {
            if (data.steerAngleActual != 0) {
                setSteerAngleActual(mc.actualSteerAngleDegrees);
            }
            if (data.switchByte != 0) {
                setSteerSwitchHigh(mc.steerSwitchHigh);
            }
            if (data.pwmDisplay != 0) {
                setLblPWMDisplay(mc.pwmDisplay);
            }
        }
    }
}

// ========== Phase 6.0.25: Separated Data Handlers ==========

void FormGPS::onNmeaDataReady(const PGNParser::ParsedData& data)
{
    // NMEA GPS data handler (~8 Hz)
    // Updates internal structures only - UpdateFixPosition() called by timerGPS at 40 Hz

    if (!data.isValid) return;

    // Phase 6.0.21.12: Ignore UDP GPS data when simulation is ON
    if (SettingsManager::instance()->menu_isSimulatorOn()) {
        return;  // Simulation mode active
    }

    // Update pn structure (internal GPS data)
    pn.fixQuality = data.quality;
    pn.satellitesTracked = data.satellites;

    // Validate GPS fix before updating position
    bool validGpsFix = (data.quality > 0 && data.satellites > 0);

    if (validGpsFix) {
        if (data.latitude != 0.0 && data.longitude != 0.0) {
            pn.latitude = data.latitude;
            pn.longitude = data.longitude;
            pn.ConvertWGS84ToLocal(pn.latitude, pn.longitude, pn.fix.northing, pn.fix.easting, this);

            // PHASE 6.0.33: Store RAW GPS position (8 Hz updates, immutable)
            // This position is NEVER modified by corrections (antenna offset, roll)
            // UpdateFixPosition() always starts from this raw position
            // Prevents cascade corrections when timer calls UpdateFixPosition() between GPS packets
            QMutexLocker lock(&m_rawGpsPositionMutex);
            m_rawGpsPosition.easting = pn.fix.easting;
            m_rawGpsPosition.northing = pn.fix.northing;
        }
        pn.altitude = data.altitude;
        pn.hdop = data.hdop;
        pn.age = data.age;
    }

    // Update speed (always, even if fix invalid)
    pn.vtgSpeed = data.speed;

    // Phase 6.0.27: Update heading in pn structure (FIXED to match legacy behavior)
    if (data.headingDual > 0) {
        // Dual antenna heading - update BOTH headingTrue and headingTrueDual
        pn.headingTrue = pn.headingTrueDual = data.headingDual;
    } else if (data.heading > 0) {
        // Single antenna heading - update headingTrue, clear headingTrueDual
        pn.headingTrue = data.heading;
        pn.headingTrueDual = 0;  // No dual antenna
    }

    // Phase 6.0.27: IMU data from NMEA (FIXED to match legacy behavior)
    // Update ahrs structure FIRST (used by calculations), then Q_PROPERTY (used by QML)
    if (data.hasIMU) {
        // Store in ahrs structure (used by calculations)
        ahrs.imuHeading = data.imuHeading;
        ahrs.imuRoll = data.imuRoll;
        ahrs.imuPitch = data.imuPitch;
        ahrs.imuYawRate = data.yawRate;

        // Update Q_PROPERTY (used by QML display)
        setImuHeading(data.imuHeading);  // 0° = north (VALID)
        setImuRoll(data.imuRoll);        // 0° = horizontal (VALID)
        setImuPitch(data.imuPitch);      // 0° = no slope (VALID)
        setYawRate(data.yawRate);        // 0°/s = no rotation (VALID)
    }

    // Phase 6.0.27 Part 3: Reset sentenceCounter on valid NMEA data (prevents "No GPS" false alarm)
    // Watchdog timer (tmrWatchdog_timeout) increments sentenceCounter every 250ms
    // MainWindow.qml shows "No GPS" warning when sentenceCounter > 29 (~7.25 seconds)
    // Must reset counter to 0 when NMEA data arrives to indicate GPS is working
    this->setSentenceCounter(0);

    // NO UpdateFixPosition() here - called by timerGPS at 40 Hz fixed rate
}

void FormGPS::onImuDataReady(const PGNParser::ParsedData& data)
{
    // External IMU module data handler (~10 Hz)
    // Updates IMU variables only - NO GPS position update

    if (!data.isValid) return;

    // Phase 6.0.21.12: Ignore UDP IMU data when simulation is ON
    if (SettingsManager::instance()->menu_isSimulatorOn()) {
        return;
    }

    // PGN 212: IMU disconnect - set sentinel values
    if (data.pgnNumber == 212) {
        setImuHeading(99999.0);  // Sentinel: IMU disconnected
        setImuRoll(88888.0);     // Sentinel: IMU disconnected
        setYawRate(0.0);
        return;
    }

    // PGN 211: External IMU data
    if (data.hasIMU) {
        setImuHeading(data.imuHeading);

        // Roll with filtering and inversion
        double rollK = data.imuRoll;
        if (ahrs.isRollInvert) rollK *= -1.0;
        rollK -= ahrs.rollZero;

        // Apply exponential filter
        double currentRoll = imuRoll();
        double filteredRoll = currentRoll * ahrs.rollFilter + rollK * (1.0 - ahrs.rollFilter);
        setImuRoll(filteredRoll);

        // Yaw rate
        if (data.yawRate != 0.0) {
            setYawRate(data.yawRate);
        }
    }

    // NO UpdateFixPosition() - IMU updates only
}

void FormGPS::onSteerDataReady(const PGNParser::ParsedData& data)
{
    // AutoSteer module feedback handler (~40 Hz throttled by timer)
    // Updates mc.* variables and AutoSteer IMU fallback
    // NO GPS position update

    if (!data.isValid || !data.hasSteerData) return;

    // PGN 253: AutoSteer status
    if (data.pgnNumber == 253) {
        // Actual steer angle from module
        mc.actualSteerAngleChart = data.steerAngleActual;
        mc.actualSteerAngleDegrees = data.steerAngleActual * 0.01;

        // IMU data from AutoSteer module (fallback if no external IMU)
        if (data.hasIMU) {
            // Heading from AutoSteer BNO085 (if valid)
            if (data.imuHeading != 9999.0) {
                setImuHeading(data.imuHeading);
            }

            // Roll from AutoSteer BNO085 (if valid, with filtering)
            if (data.imuRoll != 8888.0) {
                double rollK = data.imuRoll;
                if (ahrs.isRollInvert) rollK *= -1.0;
                rollK -= ahrs.rollZero;

                double currentRoll = imuRoll();
                double filteredRoll = currentRoll * ahrs.rollFilter + rollK * (1.0 - ahrs.rollFilter);
                setImuRoll(filteredRoll);
            }
        }

        // Switch status (work switch, steer switch)
        mc.workSwitchHigh = (data.switchByte & 0x01) != 0;
        mc.steerSwitchHigh = (data.switchByte & 0x02) != 0;

        // PWM display (motor drive 0-255)
        mc.pwmDisplay = data.pwmDisplay;

        // Reset module connection timeout counter
        setSteerModuleConnectedCounter(0);
    }

    // PGN 250: Sensor data (pressure/current)
    if (data.pgnNumber == 250) {
        mc.sensorData = data.sensorValue;
    }

    // NO UpdateFixPosition() - AutoSteer feedback only
}

// Phase 6.0.24: GPS timer callback - UpdateFixPosition() at 40 Hz fixed rate
void FormGPS::onGPSTimerTimeout()
{
    // Skip if simulation mode is active (timerSim handles position updates in simulation)
    if (SettingsManager::instance()->menu_isSimulatorOn()) {
        return;
    }

    // Call UpdateFixPosition() at 40 Hz fixed rate (independent of UDP packet arrival)
    // Uses latest data stored in pn/ahrs structures by onParsedDataReady()
    // Architecture matches simulation mode: timer-driven position updates
    UpdateFixPosition();
}
