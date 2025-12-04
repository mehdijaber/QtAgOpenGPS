// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// Main event sim comms
#include "formgps.h"
#include "classes/csim.h"
#include "qmlutil.h"
#include "classes/settingsmanager.h"
#include <QTime>

/* Callback for Simulator new position */
void FormGPS::simConnectSlots()
{
    connect(&sim, &CSim::setActualSteerAngle, this, &FormGPS::onSimNewSteerAngle, Qt::QueuedConnection);
    connect(&sim, &CSim::newPosition, this, &FormGPS::onSimNewPosition, Qt::UniqueConnection);
    connect(&timerSim, &QTimer::timeout, this, &FormGPS::onSimTimerTimeout, Qt::UniqueConnection);

    // Ensure stable gpsHz by using precise timer, accurate to 1 ms.
    // With this timer, any deviation from expected Hz is guaranteed
    // to not be in the simulator mechanism but must be in the
    // actual calculations in FormGPS.
    timerSim.setTimerType(Qt::PreciseTimer);

    if (SettingsManager::instance()->menu_isSimulatorOn()) {
        pn.latitude = sim.latitude;
        pn.longitude = sim.longitude;
        pn.headingTrue = 0;

        // PHASE 6.0.35 FIX: Initialize latStart/lonStart BEFORE first conversion
        // Problem: onSimNewPosition() calls ConvertWGS84ToLocal() BEFORE UpdateFixPosition() initializes latStart/lonStart
        // Solution: Initialize latStart/lonStart here when simulation starts (similar to real GPS mode)
        // This ensures ConvertWGS84ToLocal() uses correct reference point from first conversion
        this->setLatStart(sim.latitude);
        this->setLonStart(sim.longitude);
        pn.SetLocalMetersPerDegree(this);

        // PHASE 6.3.0 FIX: Timer will be started AFTER InterfaceProperty initialization
        // timerSim.start(20); // MOVED to initializeQMLInterfaces()
        gpsHz = 10;  //synced with 100ms timer in initializeQMLInterfaces()
        qDebug() << "Simulator ready - timer will start after QML interface initialization";
        qDebug() << "Simulation origin: latStart=" << sim.latitude << "lonStart=" << sim.longitude;
    }
}

void FormGPS::onSimNewPosition(double vtgSpeed,
                     double headingTrue,
                     double latitude,
                     double longitude, double hdop,
                     double altitude,
                     double satellitesTracked)
{
    // ✅ PHASE 6.0.21.13: Ignore simulation data when simulation is OFF
    // Prevents conflict: simulation timer still running briefly after disabling simulation
    // Symmetric to Phase 6.0.21.12 which blocks UDP when simulation ON
    // Ensures only ONE data source writes to Q_PROPERTY at a time
    if (!SettingsManager::instance()->menu_isSimulatorOn()) {
        return;  // Simulation mode disabled - ignore simulation data
    }

    // PHASE 6.0.42: Check for GPS jump in simulation mode
    // Handles REAL→SIM mode switch with field open
    // If jump detected: closes field (if open), updates latStart/lonStart, resets flags
    if (detectGPSJump(latitude, longitude)) {
        handleGPSJump(latitude, longitude);
    }

    pn.vtgSpeed = vtgSpeed;

    CVehicle::instance()->AverageTheSpeed(vtgSpeed);

    pn.headingTrue = pn.headingTrueDual = headingTrue;

    // PHASE 6.0.35 FIX: Simulation IMU heading = GPS heading (as in original C# code)
    // CSim.cs:80-81: mf.ahrs.imuHeading = mf.pn.headingTrue
    ahrs.imuHeading = pn.headingTrue;
    if (ahrs.imuHeading >= 360) ahrs.imuHeading -= 360;

    // Phase 6.3.1: Use PropertyWrapper for safe QObject access
    pn.ConvertWGS84ToLocal(latitude,longitude,pn.fix.northing,pn.fix.easting, this);
    pn.latitude = latitude;
    pn.longitude = longitude;

    // PHASE 6.0.35 FIX: Store RAW GPS position (simulation mode)
    // Symmetric to real GPS mode (formgps_position.cpp:2194-2196)
    // Prevents pn.fix reset to {0,0} when UpdateFixPosition() copies m_rawGpsPosition
    // Speed >= 1.5 km/h bypass ends → UpdateFixPosition() resets pn.fix to m_rawGpsPosition
    {
        QMutexLocker lock(&m_rawGpsPositionMutex);
        m_rawGpsPosition.easting = pn.fix.easting;
        m_rawGpsPosition.northing = pn.fix.northing;
    }

    pn.hdop = hdop;
    pn.altitude = altitude;
    pn.satellitesTracked = satellitesTracked;
    pn.fixQuality = 8;  // Simulation mode (NMEA standard value for simulation)
    pn.age = 0.0;       // No age in simulation (instant data generation)
    this->setSentenceCounter(0);

    // Phase 6.0.20: Qt 6.8 BINDABLE properties - direct setter calls replace qmlItem()->setProperty()
    // BINDABLE auto-emits property changed signals for QML reactivity
    this->setSpeedKph(vtgSpeed);
    this->setLatitude(latitude);
    this->setLongitude(longitude);
    this->setHeading(headingTrue);
    this->setAltitude(altitude);
    this->setHdop(hdop);
    this->setSatellitesTracked(satellitesTracked);

    // Phase 6.0.20 Task 24 Step 5.6: Simulation mode - missing GPS properties
    // ConvertWGS84ToLocal already called above (line 48), northing/easting available in pn.fix
   // this->setNorthing(pn.fix.northing);
   // this->setEasting(pn.fix.easting);

    // Fused heading in simulation = GPS heading (no IMU fusion needed)
    this->setFusedHeading(headingTrue);

    // Dropped sentences = 0 in perfect simulation
    this->setDroppedSentences(0);

    // GPS timing properties (frameTime, rawHz, hz) are calculated dynamically by UpdateFixPosition()
    // - formgps_position.cpp:34-47  → calculates nowHz and gpsHz from swFrame timer
    // - formgps_position.cpp:1216   → calculates frameTime with exponential filter
    // - formgps_position.cpp:1286-1287 → assigns m_hz and m_rawHz
    // No need to hardcode values here - UpdateFixPosition() provides accurate real-time measurements

    //qWarning() << "Acted on new position.";
    UpdateFixPosition();
}

void FormGPS::onSimNewSteerAngle(double steerAngleAve)
{
    // ✅ Direct update - no throttling needed with Q_PROPERTY system
    mc.actualSteerAngleDegrees = steerAngleAve;
}

/* iterate the simulator on a timer */
void FormGPS::onSimTimerTimeout()
{
    // ⚡ PHASE 6.3.0 SAFETY: Verify QML interfaces are initialized before accessing InterfaceProperty
    try {
        bool testAccess = (bool)isBtnAutoSteerOn();  // Test if InterfaceProperty works
    } catch (...) {
        qDebug() << "⏳ Simulator tick skipped - InterfaceProperty not yet ready";
        return;  // Skip this timer tick, wait for initialization
    }

    //qWarning() << "sim tick.";
    QObject *qmlobject;
    //double stepDistance = qmlobject->property("value").toReal() / 10.0 /gpsHz;
    //sim.setSimStepDistance(stepDistance);
    if (this->isDrivingRecordedPath() || (isBtnAutoSteerOn() && (CVehicle::instance()->guidanceLineDistanceOff !=32000)))
    {
        sim.DoSimTick(CVehicle::instance()->guidanceLineSteerAngle * 0.01);
    } else {
        // ⚡ SAFE QML access - handle case where simSteer is not yet loaded (Drawer content)
        static QObject* simSteerObject = nullptr;

        if (!simSteerObject) {
            simSteerObject = safeQmlItem("simSteer", 1);  // Try once, don't block
        }

        if (simSteerObject) {
            double steerAngle = (simSteerObject->property("value").toReal() - 300) * 0.1;
            sim.DoSimTick(steerAngle);
        } else {
            // Fallback: Use default steering when simSteer is not available
            sim.DoSimTick(0.0);  // No steering input
        }
    }
}

// Qt 6.8 Q_INVOKABLE wrappers for CSim methods
void FormGPS::sim_bump_speed(bool increase) {
    sim.speed_bump(increase);
}

void FormGPS::sim_zero_speed() {
    sim.speed_zero();
}

void FormGPS::sim_reset() {
    sim.reset();
}
