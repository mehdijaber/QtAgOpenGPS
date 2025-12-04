// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// Main class where everything is initialized
#include "formgps.h"
#include <QColor>
#include <QRgb>
#include "qmlutil.h"
#include "glm.h"
#include "cpgn.h"
#include <QLocale>
#include <QQuickWindow>
#include "classes/settingsmanager.h"
#include <cmath>
#include <QPixmapCache>        // Phase 6.0.45: Memory leak fixes - image cache management
#include <QCoreApplication>    // Phase 6.0.45: Memory leak fixes - sendPostedEvents for deferred deletion
#include <QEvent>              // Phase 6.0.45: Memory leak fixes - QEvent::DeferredDelete enum

FormGPS::FormGPS(QWidget *parent) : QQmlApplicationEngine(parent)
{
    qDebug() << "FormGPS constructor START";

    // Phase 6.0.24 Problem 18: Initialize Q_OBJECT_BINDABLE_PROPERTY to default values
    // CRITICAL: Q_OBJECT_BINDABLE_PROPERTY does NOT auto-initialize to false/0
    // Without explicit initialization, bool members contain random memory values
    m_isBtnAutoSteerOn = false;      // AutoSteer OFF at startup
    m_isJobStarted = false;          // No job started
    m_applicationClosing = false;    // Not closing
    m_isDrivingRecordedPath = false; // PHASE 6.0.29: Not driving recorded path at startup

    // PHASE 6.0.33: Initialize raw GPS position (two-buffer pattern)
    m_rawGpsPosition = {0.0, 0.0};   // Will be updated when first NMEA packet arrives

    // PHASE 6.0.45: Set QPixmapCache limit to prevent memory leaks
    // Default Qt cache is 10 MB which is too small for QtAgOpenGPS UI
    // Heob analysis showed 94 MB QMovie leak + 15 MB QImage leaks
    // Setting 32 MB limit prevents unbounded growth while allowing adequate caching
    QPixmapCache::setCacheLimit(32768);  // 32 MB = 32768 KB
    qDebug() << "PHASE 6.0.45: QPixmapCache limit set to 32 MB";

    qDebug() << "Setting up basic connections...";
    connect(this, &FormGPS::do_processSectionLookahead, this, &FormGPS::processSectionLookahead, Qt::QueuedConnection);
    connect(this, &FormGPS::do_processOverlapCount, this, &FormGPS::processOverlapCount, Qt::QueuedConnection);
    
    qDebug() << "ðŸ”§ Setting up AgIO service FIRST...";
    setupAgIOService();
    // Phase 6.0.27: DISABLED legacy parsedDataReady connection
    // Using separated signals (nmeaDataReady, imuDataReady, steerDataReady) instead
    // Old connection caused double updates: both onParsedDataReady() and onNmeaDataReady()
    // processed the same $PANDA sentence → race condition → satellite count fluctuation
    // Phase 6.0.21: Connect to AgIOService broadcast signal for GPS/IMU data
    // Phase 6.0.23.4: DirectConnection for real-time 40 Hz (same thread, no queue)
    // Phase 6.0.24: QueuedConnection tested but caused 100% CPU → reverted
    // connect(m_agioService, &AgIOService::parsedDataReady,
    //         this, &FormGPS::onParsedDataReady, Qt::DirectConnection);
    // qDebug() << "Phase 6.0.21: Connected to AgIOService::parsedDataReady signal";

    // Phase 6.0.25: Connect separated data signals for optimal routing
    connect(m_agioService, &AgIOService::nmeaDataReady,
            this, &FormGPS::onNmeaDataReady, Qt::DirectConnection);

    connect(m_agioService, &AgIOService::imuDataReady,
            this, &FormGPS::onImuDataReady, Qt::DirectConnection);

    connect(m_agioService, &AgIOService::steerDataReady,
            this, &FormGPS::onSteerDataReady, Qt::DirectConnection);

    qDebug() << "Phase 6.0.25: Separated NMEA/IMU/Steer signal connections established";

    qDebug() << "ðŸŽ¯ Initializing singletons...";
    // ===== CRITIQUE : Initialiser les singletons AVANT connect_classes() =====
    // CTrack will be auto-initialized via QML singleton pattern
    qDebug() << "  âœ… CTrack singleton will be auto-created by Qt";
    
    vehicle = CVehicle::instance();
    qDebug() << "  âœ… CVehicle singleton created:" << vehicle;

    qDebug() << "ðŸ”— Now calling connect_classes...";
    connect_classes(); //make all the inter-class connections (NOW trk is initialized!)

    // âš¡ CRITICAL: Initialize vehicle properties for QML access
    qDebug() << "ðŸš— Initializing vehicle properties for QML...";
    // Initialize with default values - will be updated by GPS position
    m_vehicle_xy = QVariant(QPointF(0.0, 0.0));
    m_vehicle_bounding_box = QVariant(QRectF(0.0, 0.0, 100.0, 100.0));
    qDebug() << "  ✅ vehicle_xy initialized to QPointF";
    qDebug() << "  ✅ vehicle_bounding_box initialized to QRectF";

    // Qt 6.8: Constructor ready for QML loading
    qDebug() << "âœ… FormGPS constructor core completed - ready for QML loading";

    qDebug() << "ðŸŽ¨ Now loading QML interface (AFTER constructor completion)...";
    setupGui();

    // Initialize mainWindow reference for classes that need qmlItem access
    yt.setMainWindow(mainWindow);
    vehicle->setMainWindow(mainWindow);  // Qt 6.8 FIX: Use existing pointer instead of creating new instance

    // ===== PHASE 6.3.1: PropertyWrapper initialization moved to initializeQMLInterfaces() =====
    // PropertyWrapper must be initialized AFTER QML objects are fully loaded and accessible
    qDebug() << "ðŸ”§ Phase 6.3.1: PropertyWrapper initialization will happen in initializeQMLInterfaces()";

    // Initialize AgIO singleton AFTER FormLoop is ready
    // Old QMLSettings removed - now using AgIOService singleton
    // Pure Qt 6.8 approach - factory function should be called automatically

    qDebug() << "  âœ… AgIO service initialized in main thread";
    //loadSettings(;

    // Initialize language translation system
    m_translator = new QTranslator(this);
    on_language_changed(); // Load initial translation and set up QML retranslation

    // === CRITICAL: applicationClosing connection for save_everything fix ===
    // When applicationClosing changes → automatically save with vehicle
    // Note: Using connect() instead of setBinding() to avoid recursive binding loops
    connect(this, &FormGPS::applicationClosingChanged, this, [this]() {
        if (applicationClosing()) {
            qDebug() << "🚨 applicationClosing detected - scheduling vehicle save";
            // Defer save to avoid conflicts and allow property propagation
            QTimer::singleShot(100, this, [this]() {
                qDebug() << "💾 Executing applicationClosing save with vehicle";
                FileSaveEverythingBeforeClosingField(true);  // Save vehicle on app exit
                qDebug() << "✅ applicationClosing save completed";
            });
        }
    });
    qDebug() << "🔗 applicationClosing connection established for save_everything replacement";

    qDebug() << "âœ… FormGPS full initialization completed";

    // Note: QML Component.onCompleted will trigger after setupGui() completes
    // The initialization will happen via one of our three protection mechanisms

}

// ============================================================================
// RECTANGLE PATTERN MANUAL IMPLEMENTATIONS (Qt 6.8 Required)
// ============================================================================
// Manual getters, setters, and bindables for Q_OBJECT_BINDABLE_PROPERTY

// ===== Job and Application State Properties =====
bool FormGPS::isJobStarted() const { return m_isJobStarted; }
void FormGPS::setIsJobStarted(bool isJobStarted) { m_isJobStarted = isJobStarted; }
QBindable<bool> FormGPS::bindableIsJobStarted() { return &m_isJobStarted; }

bool FormGPS::isBtnAutoSteerOn() const { return m_isBtnAutoSteerOn; }
void FormGPS::setIsBtnAutoSteerOn(bool isBtnAutoSteerOn) { m_isBtnAutoSteerOn = isBtnAutoSteerOn; }
QBindable<bool> FormGPS::bindableIsBtnAutoSteerOn() { return &m_isBtnAutoSteerOn; }

bool FormGPS::applicationClosing() const { return m_applicationClosing; }
void FormGPS::setApplicationClosing(bool applicationClosing) { m_applicationClosing = applicationClosing; }
QBindable<bool> FormGPS::bindableApplicationClosing() { return &m_applicationClosing; }

// ===== GPS Position Properties =====
double FormGPS::latitude() const { return m_latitude; }
void FormGPS::setLatitude(double latitude) { m_latitude = latitude; }
QBindable<double> FormGPS::bindableLatitude() { return &m_latitude; }

double FormGPS::longitude() const { return m_longitude; }
void FormGPS::setLongitude(double longitude) { m_longitude = longitude; }
QBindable<double> FormGPS::bindableLongitude() { return &m_longitude; }

double FormGPS::altitude() const { return m_altitude; }
void FormGPS::setAltitude(double altitude) { m_altitude = altitude; }
QBindable<double> FormGPS::bindableAltitude() { return &m_altitude; }

double FormGPS::easting() const { return m_easting; }
void FormGPS::setEasting(double easting) { m_easting = easting; }
QBindable<double> FormGPS::bindableEasting() { return &m_easting; }

double FormGPS::northing() const { return m_northing; }
void FormGPS::setNorthing(double northing) { m_northing = northing; }
QBindable<double> FormGPS::bindableNorthing() { return &m_northing; }

double FormGPS::heading() const { return m_heading; }
void FormGPS::setHeading(double heading) { m_heading = heading; }
QBindable<double> FormGPS::bindableHeading() { return &m_heading; }

QVariantList FormGPS::sectionButtonState() const {
    // Read directly from tool array - single source of truth
    QVariantList state;
    for (int i = 0; i < 65; i++) {
        state.append(static_cast<int>(tool.sectionButtonState[i]));
    }
    return state;
}
void FormGPS::setSectionButtonState(const QVariantList& value) {
    // Simple setter - update tool array and section logic directly
    qDebug() << "DEBUG FormGPS::setSectionButtonState CALLED with" << value.size() << "elements";

    for (int j = 0; j < qMin(value.size(), 65); j++) {
        int buttonState = value[j].toInt();
        tool.sectionButtonState[j] = static_cast<btnStates>(buttonState);

        // Update section logic only for active sections
        if (j < tool.numOfSections) {
            bool newSectionOn = (buttonState == btnStates::Auto || buttonState == btnStates::On);
            tool.section[j].isSectionOn = newSectionOn;
            tool.section[j].sectionOnRequest = newSectionOn;
            tool.section[j].sectionOffRequest = !newSectionOn;
        }
    }

    // Simple BINDABLE notification - no recursive calls possible
    m_sectionButtonState = value;
}

// syncSectionButtonStateToQML() REMOVED - Qt BINDABLE handles automatic synchronization

QBindable<QVariantList> FormGPS::bindableSectionButtonState() { return &m_sectionButtonState; }

double FormGPS::speedKph() const { return m_speedKph; }
void FormGPS::setSpeedKph(double speedKph) {
    m_speedKph = speedKph;

    // ⚡ PHASE 6.0.20 AutoSteer Protection: Automatic speed-based deactivation
    // Covers both simulation and real GPS modes (single entry point)
    if (m_isBtnAutoSteerOn) {
        auto* settings = SettingsManager::instance();
        if (speedKph < settings->as_minSteerSpeed() ||
            speedKph > settings->as_maxSteerSpeed()) {
            setIsBtnAutoSteerOn(false);
        }
    }
}
QBindable<double> FormGPS::bindableSpeedKph() { return &m_speedKph; }

double FormGPS::fusedHeading() const { return m_fusedHeading; }
void FormGPS::setFusedHeading(double fusedHeading) { m_fusedHeading = fusedHeading; }
QBindable<double> FormGPS::bindableFusedHeading() { return &m_fusedHeading; }

// ===== Tool Position Properties =====
double FormGPS::toolEasting() const { return m_toolEasting; }
void FormGPS::setToolEasting(double toolEasting) { m_toolEasting = toolEasting; }
QBindable<double> FormGPS::bindableToolEasting() { return &m_toolEasting; }

double FormGPS::toolNorthing() const { return m_toolNorthing; }
void FormGPS::setToolNorthing(double toolNorthing) { m_toolNorthing = toolNorthing; }
QBindable<double> FormGPS::bindableToolNorthing() { return &m_toolNorthing; }

double FormGPS::toolHeading() const { return m_toolHeading; }
void FormGPS::setToolHeading(double toolHeading) { m_toolHeading = toolHeading; }
QBindable<double> FormGPS::bindableToolHeading() { return &m_toolHeading; }

double FormGPS::offlineDistance() const { return m_offlineDistance; }
void FormGPS::setOfflineDistance(double offlineDistance) { m_offlineDistance = offlineDistance; }
QBindable<double> FormGPS::bindableOfflineDistance() { return &m_offlineDistance; }

// ===== Steering Properties =====
double FormGPS::steerAngleActual() const { return m_steerAngleActual; }
void FormGPS::setSteerAngleActual(double steerAngleActual) { m_steerAngleActual = steerAngleActual; }
QBindable<double> FormGPS::bindableSteerAngleActual() { return &m_steerAngleActual; }

double FormGPS::steerAngleSet() const { return m_steerAngleSet; }
void FormGPS::setSteerAngleSet(double steerAngleSet) { m_steerAngleSet = steerAngleSet; }
QBindable<double> FormGPS::bindableSteerAngleSet() { return &m_steerAngleSet; }

int FormGPS::lblPWMDisplay() const { return m_lblPWMDisplay; }
void FormGPS::setLblPWMDisplay(int lblPWMDisplay) { m_lblPWMDisplay = lblPWMDisplay; }
QBindable<int> FormGPS::bindableLblPWMDisplay() { return &m_lblPWMDisplay; }

double FormGPS::calcSteerAngleInner() const { return m_calcSteerAngleInner; }
void FormGPS::setCalcSteerAngleInner(double calcSteerAngleInner) { m_calcSteerAngleInner = calcSteerAngleInner; }
QBindable<double> FormGPS::bindableCalcSteerAngleInner() { return &m_calcSteerAngleInner; }

double FormGPS::calcSteerAngleOuter() const { return m_calcSteerAngleOuter; }
void FormGPS::setCalcSteerAngleOuter(double calcSteerAngleOuter) { m_calcSteerAngleOuter = calcSteerAngleOuter; }
QBindable<double> FormGPS::bindableCalcSteerAngleOuter() { return &m_calcSteerAngleOuter; }

double FormGPS::diameter() const { return m_diameter; }
void FormGPS::setDiameter(double diameter) { m_diameter = diameter; }
QBindable<double> FormGPS::bindableDiameter() { return &m_diameter; }

// ===== IMU Properties =====
double FormGPS::imuRoll() const { return m_imuRoll; }
void FormGPS::setImuRoll(double imuRoll) { m_imuRoll = imuRoll; }
QBindable<double> FormGPS::bindableImuRoll() { return &m_imuRoll; }

double FormGPS::imuPitch() const { return m_imuPitch; }
void FormGPS::setImuPitch(double imuPitch) { m_imuPitch = imuPitch; }
QBindable<double> FormGPS::bindableImuPitch() { return &m_imuPitch; }

double FormGPS::imuHeading() const { return m_imuHeading; }
void FormGPS::setImuHeading(double imuHeading) { m_imuHeading = imuHeading; }
QBindable<double> FormGPS::bindableImuHeading() { return &m_imuHeading; }

double FormGPS::imuRollDegrees() const { return m_imuRollDegrees; }
void FormGPS::setImuRollDegrees(double imuRollDegrees) { m_imuRollDegrees = imuRollDegrees; }
QBindable<double> FormGPS::bindableImuRollDegrees() { return &m_imuRollDegrees; }

double FormGPS::imuAngVel() const { return m_imuAngVel; }
void FormGPS::setImuAngVel(double imuAngVel) { m_imuAngVel = imuAngVel; }
QBindable<double> FormGPS::bindableImuAngVel() { return &m_imuAngVel; }

double FormGPS::yawRate() const { return m_yawRate; }
void FormGPS::setYawRate(double yawRate) { m_yawRate = yawRate; }
QBindable<double> FormGPS::bindableYawRate() { return &m_yawRate; }

// ===== GPS Quality Properties =====
double FormGPS::hdop() const { return m_hdop; }
void FormGPS::setHdop(double hdop) { m_hdop = hdop; }
QBindable<double> FormGPS::bindableHdop() { return &m_hdop; }

double FormGPS::age() const { return m_age; }
void FormGPS::setAge(double age) { m_age = age; }
QBindable<double> FormGPS::bindableAge() { return &m_age; }

int FormGPS::fixQuality() const { return m_fixQuality; }
void FormGPS::setFixQuality(int fixQuality) { m_fixQuality = fixQuality; }
QBindable<int> FormGPS::bindableFixQuality() { return &m_fixQuality; }

int FormGPS::satellitesTracked() const { return m_satellitesTracked; }
void FormGPS::setSatellitesTracked(int satellitesTracked) { m_satellitesTracked = satellitesTracked; }
QBindable<int> FormGPS::bindableSatellitesTracked() { return &m_satellitesTracked; }

double FormGPS::hz() const { return m_hz; }
void FormGPS::setHz(double hz) { m_hz = hz; }
QBindable<double> FormGPS::bindableHz() { return &m_hz; }

double FormGPS::rawHz() const { return m_rawHz; }
void FormGPS::setRawHz(double rawHz) { m_rawHz = rawHz; }
QBindable<double> FormGPS::bindableRawHz() { return &m_rawHz; }

// ===== Blockage Properties =====
double FormGPS::blockage_avg() const { return m_blockage_avg; }
void FormGPS::setBlockage_avg(double blockage_avg) { m_blockage_avg = blockage_avg; }
QBindable<double> FormGPS::bindableBlockage_avg() { return &m_blockage_avg; }

double FormGPS::blockage_min1() const { return m_blockage_min1; }
void FormGPS::setBlockage_min1(double blockage_min1) { m_blockage_min1 = blockage_min1; }
QBindable<double> FormGPS::bindableBlockage_min1() { return &m_blockage_min1; }

double FormGPS::blockage_min2() const { return m_blockage_min2; }
void FormGPS::setBlockage_min2(double blockage_min2) { m_blockage_min2 = blockage_min2; }
QBindable<double> FormGPS::bindableBlockage_min2() { return &m_blockage_min2; }

double FormGPS::blockage_max() const { return m_blockage_max; }
void FormGPS::setBlockage_max(double blockage_max) { m_blockage_max = blockage_max; }
QBindable<double> FormGPS::bindableBlockage_max() { return &m_blockage_max; }

int FormGPS::blockage_min1_i() const { return m_blockage_min1_i; }
void FormGPS::setBlockage_min1_i(int blockage_min1_i) { m_blockage_min1_i = blockage_min1_i; }
QBindable<int> FormGPS::bindableBlockage_min1_i() { return &m_blockage_min1_i; }

int FormGPS::blockage_min2_i() const { return m_blockage_min2_i; }
void FormGPS::setBlockage_min2_i(int blockage_min2_i) { m_blockage_min2_i = blockage_min2_i; }
QBindable<int> FormGPS::bindableBlockage_min2_i() { return &m_blockage_min2_i; }

int FormGPS::blockage_max_i() const { return m_blockage_max_i; }
void FormGPS::setBlockage_max_i(int blockage_max_i) { m_blockage_max_i = blockage_max_i; }
QBindable<int> FormGPS::bindableBlockage_max_i() { return &m_blockage_max_i; }

bool FormGPS::blockage_blocked() const { return m_blockage_blocked; }
void FormGPS::setBlockage_blocked(bool blockage_blocked) { m_blockage_blocked = blockage_blocked; }
QBindable<bool> FormGPS::bindableBlockage_blocked() { return &m_blockage_blocked; }

double FormGPS::avgPivDistance() const { return m_avgPivDistance; }
void FormGPS::setAvgPivDistance(double avgPivDistance) { m_avgPivDistance = avgPivDistance; }
QBindable<double> FormGPS::bindableAvgPivDistance() { return &m_avgPivDistance; }

double FormGPS::frameTime() const { return m_frameTime; }
void FormGPS::setFrameTime(double frameTime) { m_frameTime = frameTime; }
QBindable<double> FormGPS::bindableFrameTime() { return &m_frameTime; }

// ===== Turn and Navigation Properties =====
double FormGPS::distancePivotToTurnLine() const { return m_distancePivotToTurnLine; }
void FormGPS::setDistancePivotToTurnLine(double distancePivotToTurnLine) { m_distancePivotToTurnLine = distancePivotToTurnLine; }
QBindable<double> FormGPS::bindableDistancePivotToTurnLine() { return &m_distancePivotToTurnLine; }

bool FormGPS::isYouTurnRight() const { return m_isYouTurnRight; }
void FormGPS::setIsYouTurnRight(bool isYouTurnRight) { m_isYouTurnRight = isYouTurnRight; }
QBindable<bool> FormGPS::bindableIsYouTurnRight() { return &m_isYouTurnRight; }

bool FormGPS::isYouTurnTriggered() const { return m_isYouTurnTriggered; }
void FormGPS::setIsYouTurnTriggered(bool isYouTurnTriggered) { m_isYouTurnTriggered = isYouTurnTriggered; }
QBindable<bool> FormGPS::bindableIsYouTurnTriggered() { return &m_isYouTurnTriggered; }

int FormGPS::current_trackNum() const { return m_current_trackNum; }
void FormGPS::setCurrent_trackNum(int current_trackNum) { m_current_trackNum = current_trackNum; }
QBindable<int> FormGPS::bindableCurrent_trackNum() { return &m_current_trackNum; }

int FormGPS::track_idx() const { return m_track_idx; }
void FormGPS::setTrack_idx(int track_idx) { m_track_idx = track_idx; }
QBindable<int> FormGPS::bindableTrack_idx() { return &m_track_idx; }

double FormGPS::lblmodeActualXTE() const { return m_lblmodeActualXTE; }
void FormGPS::setLblmodeActualXTE(double lblmodeActualXTE) { m_lblmodeActualXTE = lblmodeActualXTE; }
QBindable<double> FormGPS::bindableLblmodeActualXTE() { return &m_lblmodeActualXTE; }

double FormGPS::lblmodeActualHeadingError() const { return m_lblmodeActualHeadingError; }
void FormGPS::setLblmodeActualHeadingError(double lblmodeActualHeadingError) { m_lblmodeActualHeadingError = lblmodeActualHeadingError; }
QBindable<double> FormGPS::bindableLblmodeActualHeadingError() { return &m_lblmodeActualHeadingError; }

double FormGPS::toolLatitude() const { return m_toolLatitude; }
void FormGPS::setToolLatitude(double toolLatitude) { m_toolLatitude = toolLatitude; }
QBindable<double> FormGPS::bindableToolLatitude() { return &m_toolLatitude; }

double FormGPS::toolLongitude() const { return m_toolLongitude; }
void FormGPS::setToolLongitude(double toolLongitude) { m_toolLongitude = toolLongitude; }
QBindable<double> FormGPS::bindableToolLongitude() { return &m_toolLongitude; }

int FormGPS::sampleCount() const { return m_sampleCount; }
void FormGPS::setSampleCount(int sampleCount) { m_sampleCount = sampleCount; }
QBindable<int> FormGPS::bindableSampleCount() { return &m_sampleCount; }

double FormGPS::confidenceLevel() const { return m_confidenceLevel; }
void FormGPS::setConfidenceLevel(double confidenceLevel) { m_confidenceLevel = confidenceLevel; }
QBindable<double> FormGPS::bindableConfidenceLevel() { return &m_confidenceLevel; }

bool FormGPS::hasValidRecommendation() const { return m_hasValidRecommendation; }
void FormGPS::setHasValidRecommendation(bool hasValidRecommendation) { m_hasValidRecommendation = hasValidRecommendation; }
QBindable<bool> FormGPS::bindableHasValidRecommendation() { return &m_hasValidRecommendation; }

bool FormGPS::startSA() const { return m_startSA; }
void FormGPS::setStartSA(bool startSA) { m_startSA = startSA; }
QBindable<bool> FormGPS::bindableStartSA() { return &m_startSA; }

QVariant FormGPS::vehicle_xy() const { return m_vehicle_xy; }
void FormGPS::setVehicle_xy(const QVariant& vehicle_xy) { m_vehicle_xy = vehicle_xy; }
QBindable<QVariant> FormGPS::bindableVehicle_xy() { return &m_vehicle_xy; }

QVariant FormGPS::vehicle_bounding_box() const { return m_vehicle_bounding_box; }
void FormGPS::setVehicle_bounding_box(const QVariant& vehicle_bounding_box) { m_vehicle_bounding_box = vehicle_bounding_box; }
QBindable<QVariant> FormGPS::bindableVehicle_bounding_box() { return &m_vehicle_bounding_box; }

// ===== IMU and Switch Properties =====
bool FormGPS::steerSwitchHigh() const { return m_steerSwitchHigh; }
void FormGPS::setSteerSwitchHigh(bool steerSwitchHigh) { m_steerSwitchHigh = steerSwitchHigh; }
QBindable<bool> FormGPS::bindableSteerSwitchHigh() { return &m_steerSwitchHigh; }

bool FormGPS::imuCorrected() const { return m_imuCorrected; }
void FormGPS::setImuCorrected(bool imuCorrected) { m_imuCorrected = imuCorrected; }
QBindable<bool> FormGPS::bindableImuCorrected() { return &m_imuCorrected; }

QString FormGPS::lblCalcSteerAngleInner() const { return m_lblCalcSteerAngleInner; }
void FormGPS::setLblCalcSteerAngleInner(const QString &value) { m_lblCalcSteerAngleInner = value; }
QBindable<QString> FormGPS::bindableLblCalcSteerAngleInner() { return &m_lblCalcSteerAngleInner; }

QString FormGPS::lblDiameter() const { return m_lblDiameter; }
void FormGPS::setLblDiameter(const QString &value) { m_lblDiameter = value; }
QBindable<QString> FormGPS::bindableLblDiameter() { return &m_lblDiameter; }

int FormGPS::droppedSentences() const { return m_droppedSentences; }
void FormGPS::setDroppedSentences(int value) { m_droppedSentences = value; }
QBindable<int> FormGPS::bindableDroppedSentences() { return &m_droppedSentences; }

// ===== Area and Boundary Properties =====
double FormGPS::areaOuterBoundary() const { return m_areaOuterBoundary; }
void FormGPS::setAreaOuterBoundary(double areaOuterBoundary) { m_areaOuterBoundary = areaOuterBoundary; }
QBindable<double> FormGPS::bindableAreaOuterBoundary() { return &m_areaOuterBoundary; }

double FormGPS::areaBoundaryOuterLessInner() const { return m_areaBoundaryOuterLessInner; }
void FormGPS::setAreaBoundaryOuterLessInner(double areaBoundaryOuterLessInner) { m_areaBoundaryOuterLessInner = areaBoundaryOuterLessInner; }
QBindable<double> FormGPS::bindableAreaBoundaryOuterLessInner() { return &m_areaBoundaryOuterLessInner; }

double FormGPS::workedAreaTotal() const { return m_workedAreaTotal; }
void FormGPS::setWorkedAreaTotal(double workedAreaTotal) { m_workedAreaTotal = workedAreaTotal; }
QBindable<double> FormGPS::bindableWorkedAreaTotal() { return &m_workedAreaTotal; }

double FormGPS::workedAreaTotalUser() const { return m_workedAreaTotalUser; }
void FormGPS::setWorkedAreaTotalUser(double workedAreaTotalUser) { m_workedAreaTotalUser = workedAreaTotalUser; }
QBindable<double> FormGPS::bindableWorkedAreaTotalUser() { return &m_workedAreaTotalUser; }

double FormGPS::distanceUser() const { return m_distanceUser; }
void FormGPS::setDistanceUser(double distanceUser) { m_distanceUser = distanceUser; }
QBindable<double> FormGPS::bindableDistanceUser() { return &m_distanceUser; }

double FormGPS::actualAreaCovered() const { return m_actualAreaCovered; }
void FormGPS::setActualAreaCovered(double actualAreaCovered) { m_actualAreaCovered = actualAreaCovered; }
QBindable<double> FormGPS::bindableActualAreaCovered() { return &m_actualAreaCovered; }

double FormGPS::userSquareMetersAlarm() const { return m_userSquareMetersAlarm; }
void FormGPS::setUserSquareMetersAlarm(double userSquareMetersAlarm) { m_userSquareMetersAlarm = userSquareMetersAlarm; }
QBindable<double> FormGPS::bindableUserSquareMetersAlarm() { return &m_userSquareMetersAlarm; }

// ===== Button State Properties =====
bool FormGPS::isContourBtnOn() const { return m_isContourBtnOn; }
void FormGPS::setIsContourBtnOn(bool isContourBtnOn) { m_isContourBtnOn = isContourBtnOn; }
QBindable<bool> FormGPS::bindableIsContourBtnOn() { return &m_isContourBtnOn; }

bool FormGPS::isYouTurnBtnOn() const { return m_isYouTurnBtnOn; }
void FormGPS::setIsYouTurnBtnOn(bool isYouTurnBtnOn) { m_isYouTurnBtnOn = isYouTurnBtnOn; }
QBindable<bool> FormGPS::bindableIsYouTurnBtnOn() { return &m_isYouTurnBtnOn; }

int FormGPS::sensorData() const { return m_sensorData; }
void FormGPS::setSensorData(int sensorData) { m_sensorData = sensorData; }
QBindable<int> FormGPS::bindableSensorData() { return &m_sensorData; }

bool FormGPS::btnIsContourLocked() const { return m_btnIsContourLocked; }
void FormGPS::setBtnIsContourLocked(bool btnIsContourLocked) { m_btnIsContourLocked = btnIsContourLocked; }
QBindable<bool> FormGPS::bindableBtnIsContourLocked() { return &m_btnIsContourLocked; }

double FormGPS::latStart() const { return m_latStart.value(); }
void FormGPS::setLatStart(double latStart) {
    qDebug() << "[GEODETIC_DEBUG] setLatStart called with value:" << latStart;
    m_latStart = latStart;  // ✅ Qt 6.8 Official Doc - Direct assignment emits signal
    updateMPerDegreeLat();
    qDebug() << "[GEODETIC_DEBUG] After set - m_latStart:" << m_latStart.value() << "mPerDegreeLat:" << m_mPerDegreeLat;
}
QBindable<double> FormGPS::bindableLatStart() { return &m_latStart; }

double FormGPS::lonStart() const { return m_lonStart.value(); }
void FormGPS::setLonStart(double lonStart) {
    qDebug() << "[GEODETIC_DEBUG] setLonStart called with value:" << lonStart;
    m_lonStart = lonStart;  // ✅ Qt 6.8 Official Doc - Direct assignment emits signal
}
QBindable<double> FormGPS::bindableLonStart() { return &m_lonStart; }

// Geodetic Conversion - Phase 6.0.20 Task 24 Step 3.5
// mPerDegreeLat getter is inline in .h (simple member variable, no BINDABLE overhead)

void FormGPS::updateMPerDegreeLat() {
    // WGS84 geodetic formula for meters per degree latitude
    // Based on latStart (fixed reference point for the field)
    double latStart = m_latStart.value();
    m_mPerDegreeLat = 111132.92 - 559.82 * cos(2.0 * latStart * 0.01745329251994329576923690766743)
                    + 1.175 * cos(4.0 * latStart * 0.01745329251994329576923690766743)
                    - 0.0023 * cos(6.0 * latStart * 0.01745329251994329576923690766743);
    // Direct assignment - no signal emission (C++ only, not exposed to QML)
}

// Geodetic Conversion Functions - Phase 6.0.20 Task 24 Step 3.5
// Wrappers for QML - delegate to CNMEA for actual conversion logic
QVariantList FormGPS::convertLocalToWGS84(double northing, double easting) {
    double outLat, outLon;
    // Call CNMEA conversion function (single source of truth)
    pn.ConvertLocalToWGS84(northing, easting, outLat, outLon, this);
    // Return [latitude, longitude] as QVariantList for QML
    return QVariantList() << outLat << outLon;
}

QVariantList FormGPS::convertWGS84ToLocal(double latitude, double longitude) {
    double outNorthing, outEasting;
    // Call CNMEA conversion function (single source of truth)
    pn.ConvertWGS84ToLocal(latitude, longitude, outNorthing, outEasting, this);
    // Return [northing, easting] as QVariantList for QML
    return QVariantList() << outNorthing << outEasting;
}

uint FormGPS::sentenceCounter() const { return m_sentenceCounter; }
void FormGPS::setSentenceCounter(uint value) { m_sentenceCounter = value; }
QBindable<uint> FormGPS::bindableSentenceCounter() { return &m_sentenceCounter; }

// GPS/IMU Heading - Phase 6.0.20 Task 24 Step 2
double FormGPS::gpsHeading() const { return m_gpsHeading; }
void FormGPS::setGpsHeading(double value) { m_gpsHeading = value; }
QBindable<double> FormGPS::bindableGpsHeading() { return &m_gpsHeading; }

bool FormGPS::isReverseWithIMU() const { return m_isReverseWithIMU; }
void FormGPS::setIsReverseWithIMU(bool value) { m_isReverseWithIMU = value; }
QBindable<bool> FormGPS::bindableIsReverseWithIMU() { return &m_isReverseWithIMU; }

// Module Connection - Phase 6.0.20 Task 24 Step 3.2
int FormGPS::steerModuleConnectedCounter() const { return m_steerModuleConnectedCounter; }
void FormGPS::setSteerModuleConnectedCounter(int value) { m_steerModuleConnectedCounter = value; }
QBindable<int> FormGPS::bindableSteerModuleConnectedCounter() { return &m_steerModuleConnectedCounter; }

int FormGPS::autoBtnState() const { return m_autoBtnState; }
void FormGPS::setAutoBtnState(int autoBtnState) {
    m_autoBtnState = autoBtnState;

    // PHASE 6.0.36: When Master Auto button activated, set all sections to Auto mode
    // This allows automatic section activation based on boundary and coverage
    // Only changes sections currently in Off state - respects manual On state
    if (autoBtnState == btnStates::Auto && isJobStarted()) {
        for (int j = 0; j < tool.numOfSections; j++) {
            if (tool.sectionButtonState[j] == btnStates::Off) {
                tool.sectionButtonState[j] = btnStates::Auto;
                tool.section[j].sectionBtnState = btnStates::Auto;
            }
        }
    }
    // When Master Auto turned off, set all Auto sections back to Off
    // Respects manual On state
    else if (autoBtnState == btnStates::Off && isJobStarted()) {
        for (int j = 0; j < tool.numOfSections; j++) {
            if (tool.sectionButtonState[j] == btnStates::Auto) {
                tool.sectionButtonState[j] = btnStates::Off;
                tool.section[j].sectionBtnState = btnStates::Off;
            }
        }
    }
}
QBindable<int> FormGPS::bindableAutoBtnState() { return &m_autoBtnState; }

int FormGPS::manualBtnState() const { return m_manualBtnState; }
void FormGPS::setManualBtnState(int manualBtnState) { m_manualBtnState = manualBtnState; }
QBindable<int> FormGPS::bindableManualBtnState() { return &m_manualBtnState; }

bool FormGPS::autoTrackBtnState() const { return m_autoTrackBtnState; }
void FormGPS::setAutoTrackBtnState(bool autoTrackBtnState) { m_autoTrackBtnState = autoTrackBtnState; }
QBindable<bool> FormGPS::bindableAutoTrackBtnState() { return &m_autoTrackBtnState; }

bool FormGPS::autoYouturnBtnState() const { return m_autoYouturnBtnState; }
void FormGPS::setAutoYouturnBtnState(bool autoYouturnBtnState) { m_autoYouturnBtnState = autoYouturnBtnState; }
QBindable<bool> FormGPS::bindableAutoYouturnBtnState() { return &m_autoYouturnBtnState; }

bool FormGPS::isPatchesChangingColor() const { return m_isPatchesChangingColor; }
void FormGPS::setIsPatchesChangingColor(bool isPatchesChangingColor) { m_isPatchesChangingColor = isPatchesChangingColor; }
QBindable<bool> FormGPS::bindableIsPatchesChangingColor() { return &m_isPatchesChangingColor; }

bool FormGPS::isOutOfBounds() const { return m_isOutOfBounds; }
void FormGPS::setIsOutOfBounds(bool isOutOfBounds) { m_isOutOfBounds = isOutOfBounds; }
QBindable<bool> FormGPS::bindableIsOutOfBounds() { return &m_isOutOfBounds; }

bool FormGPS::isHeadlandOn() const { return m_isHeadlandOn; }
void FormGPS::setIsHeadlandOn(bool isHeadlandOn) { m_isHeadlandOn = isHeadlandOn; }
QBindable<bool> FormGPS::bindableIsHeadlandOn() { return &m_isHeadlandOn; }

double FormGPS::createBndOffset() const { return m_createBndOffset; }
void FormGPS::setCreateBndOffset(double createBndOffset) { m_createBndOffset = createBndOffset; }
QBindable<double> FormGPS::bindableCreateBndOffset() { return &m_createBndOffset; }

bool FormGPS::isDrawRightSide() const { return m_isDrawRightSide; }
void FormGPS::setIsDrawRightSide(bool isDrawRightSide) { m_isDrawRightSide = isDrawRightSide; }
QBindable<bool> FormGPS::bindableIsDrawRightSide() { return &m_isDrawRightSide; }

bool FormGPS::isDrivingRecordedPath() const { return m_isDrivingRecordedPath; }
void FormGPS::setIsDrivingRecordedPath(bool isDrivingRecordedPath) { m_isDrivingRecordedPath = isDrivingRecordedPath; }
QBindable<bool> FormGPS::bindableIsDrivingRecordedPath() { return &m_isDrivingRecordedPath; }

QString FormGPS::recordedPathName() const { return m_recordedPathName; }
void FormGPS::setRecordedPathName(const QString& value) { m_recordedPathName = value; }
QBindable<QString> FormGPS::bindableRecordedPathName() { return &m_recordedPathName; }

// Boundary State
bool FormGPS::boundaryIsRecording() const { return m_boundaryIsRecording; }
void FormGPS::setBoundaryIsRecording(bool value) { m_boundaryIsRecording = value; }
QBindable<bool> FormGPS::bindableBoundaryIsRecording() { return &m_boundaryIsRecording; }

double FormGPS::boundaryArea() const { return m_boundaryArea; }
void FormGPS::setBoundaryArea(double value) { m_boundaryArea = value; }
QBindable<double> FormGPS::bindableBoundaryArea() { return &m_boundaryArea; }

int FormGPS::boundaryPointCount() const { return m_boundaryPointCount; }
void FormGPS::setBoundaryPointCount(int value) { m_boundaryPointCount = value; }
QBindable<int> FormGPS::bindableBoundaryPointCount() { return &m_boundaryPointCount; }

FormGPS::~FormGPS()
{
    qDebug() << "FormGPS destructor START - cleaning up resources";

    // Phase 6.0.45: Memory leak fixes - 5-step QML cleanup sequence

    // Step 1: Clear QML engine component cache to free QQmlObjectCreator instances
    // Addresses: 90,513 leaked QQmlObjectCreator::createInstance() allocations
    clearComponentCache();
    qDebug() << "QML component cache cleared";

    // Step 2: Trigger JavaScript garbage collection to free QML objects
    // Addresses: 62,426 leaked QQmlObjectCreator::populateInstance() allocations
    collectGarbage();
    qDebug() << "QML garbage collection completed";

    // Step 3: Clear Qt image caches to free QPixmap/QImage allocations
    // Addresses: 94 MB QMovie leak + 15 MB QImage leaks
    QPixmapCache::clear();
    qDebug() << "QPixmapCache cleared";

    // Step 4: Disconnect all signal/slot connections to prevent dangling references
    // Addresses: 62,194 leaked QQmlObjectCreator::setPropertyBinding() allocations
    disconnect();
    qDebug() << "All signals disconnected";

    // Step 5: Clean up AgIO service (existing cleanup)
    cleanupAgIOService();
    qDebug() << "AgIO service cleaned up";

    // Step 6: Process pending deleteLater() calls to ensure deferred deletions complete
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qDebug() << "Pending deletions processed";

    // Clean up translator (automatically cleaned by parent)
    // translator will be deleted automatically by parent object

    qDebug() << "FormGPS destructor COMPLETE";
}

void FormGPS::processOverlapCount()
{
    if (isJobStarted())
    {
        int once = 0;
        int twice = 0;
        int more = 0;
        int level = 0;
        double total = 0;
        double total2 = 0;

        //50, 96, 112
        for (int i = 0; i < 400 * 400; i++)
        {

            if (overPixels[i].red > 105)
            {
                more++;
                level = overPixels[i].red;
            }
            else if (overPixels[i].red > 85)
            {
                twice++;
                level = overPixels[i].red;
            }
            else if (overPixels[i].red > 50)
            {
                once++;
            }
        }
        total = once + twice + more;
        total2 = total + twice + more + more;

        if (total2 > 0)
        {
            this->setActualAreaCovered( (total / total2 * (double)this->workedAreaTotal()));
            fd.overlapPercent = ((1 - total / total2) * 100);
        }
        else
        {
            this->setActualAreaCovered( 0);
            fd.overlapPercent = 0;
        }
    }
}

// PHASE 6.0.40: Reset GPS state when switching between sim/real modes
// Prevents gray screen bug when toggling between simulation and real GPS
void FormGPS::ResetGPSState(bool toSimMode)
{
    // PHASE 6.0.42.7: Save and close field before mode switch
    // Field data is tied to current coordinate system (latStart/lonStart)
    // Switching modes changes coordinate reference → must save field before switch
    // Same logic as GPS jump detection (handleGPSJump)
    if (isJobStarted()) {
        qDebug() << "Field open during mode switch - saving and closing";
        FileSaveEverythingBeforeClosingField(false);  // Save all field data
        JobClose();  // Close field properly
        qDebug() << "Field closed successfully before mode switch";
    }

    // Reset initialization flags
    isGPSPositionInitialized = false;
    isFirstFixPositionSet = false;
    isFirstHeadingSet = false;
    startCounter = 0;

    // PHASE 6.0.42: Reset guidance line offset when switching modes
    // Old offset from previous mode is invalid in new coordinate system
    CVehicle::instance()->guidanceLineDistanceOff = 32000;
    CVehicle::instance()->guidanceLineSteerAngle = 0;

    // PHASE 6.0.42: Reset stepFixPts[] for heading calculation
    // Old position history from previous coordinate system is invalid
    // Must accumulate 3 new points to calculate heading automatically
    for (int i = 0; i < totalFixSteps; i++) {
        stepFixPts[i].isSet = 0;
    }

    // Reset sentence counter to prevent "No GPS" false alarm
    setSentenceCounter(0);

    if (toSimMode) {
        // Initialize with simulation coordinates
        pn.latitude = sim.latitude;
        pn.longitude = sim.longitude;
        pn.headingTrue = 0;

        // CRITICAL: Initialize latStart/lonStart for sim mode
        // Without this, ConvertWGS84ToLocal uses wrong reference point
        setLatStart(sim.latitude);
        setLonStart(sim.longitude);
        pn.SetLocalMetersPerDegree(this);

        // Convert sim position to local coords
        pn.ConvertWGS84ToLocal(sim.latitude, sim.longitude, pn.fix.northing, pn.fix.easting, this);

        // Initialize raw GPS position for sim (used for heading calculation)
        {
            QMutexLocker lock(&m_rawGpsPositionMutex);
            m_rawGpsPosition.easting = pn.fix.easting;
            m_rawGpsPosition.northing = pn.fix.northing;
        }

        // PHASE 6.0.42: Update last known position for jump detection
        // Prevents false jump detection when switching to simulation
        m_lastKnownLatitude = sim.latitude;
        m_lastKnownLongitude = sim.longitude;

        // PHASE 6.0.42.6: Reset IMU values for simulation mode
        // Simulation = perfect flat terrain, no roll/pitch/yaw
        // Prevents old real-mode IMU values from corrupting simulation position calculations
        // Old IMU values would cause incorrect roll corrections and sidehill compensation
        ahrs.imuRoll = 0.0;     // Flat terrain (no roll)
        ahrs.imuPitch = 0.0;    // No slope (no pitch)
        ahrs.imuYawRate = 0.0;  // No rotation (no yaw rate)
        // Note: ahrs.imuHeading updated by onSimNewPosition() = pn.headingTrue (line 69)

        // PHASE 6.0.42.1: Mark position as initialized since we just set latStart/lonStart above
        // Allows startCounter to increment immediately instead of wasting 1 cycle in InitializeFirstFewGPSPositions()
        isFirstFixPositionSet = true;
    } else {
        // PHASE 6.0.42.3: Real mode - INVALIDATE position to prevent premature initialization
        // Problem: If we keep SIM coordinates, InitializeFirstFewGPSPositions() initializes
        // with stale SIM coords BEFORE real GPS arrives → gray screen when GPS finally arrives
        // Solution: Reset pn.latitude/longitude to 0 to force waiting for real GPS
        // This makes behavior identical to application startup (REAL + UDP ON)

        // PHASE 6.0.42.4: ALSO reset m_lastKnownLatitude/longitude to prevent false GPS jump detection
        // Problem: If we keep SIM coords in m_lastKnown*, detectGPSJump() triggers when real GPS arrives
        // → handleGPSJump() resets everything → gray screen even with UDP ON
        // Solution: Reset m_lastKnown* to 0 → first real GPS treated as "first fix", not a "jump"
        m_lastKnownLatitude = 0;
        m_lastKnownLongitude = 0;

        // Invalidate current position - forces waiting for real GPS data
        pn.latitude = 0;
        pn.longitude = 0;
        pn.fix.easting = 0;
        pn.fix.northing = 0;

        // Reset raw GPS position - will be updated when real GPS data arrives
        {
            QMutexLocker lock(&m_rawGpsPositionMutex);
            m_rawGpsPosition.easting = 0;
            m_rawGpsPosition.northing = 0;
        }

        // PHASE 6.0.41: Force latStart/lonStart reinitialization even if field is open
        // Prevents gray screen when GPS arrives after mode switch with open field
        m_forceGPSReinitialization = true;

        // PHASE 6.0.42.2: DON'T set isFirstFixPositionSet = true in REAL mode
        // Because we don't have valid GPS coordinates yet (UDP OFF scenario)
        // InitializeFirstFewGPSPositions() will wait for real GPS to arrive
    }

    // Reset previous positions for heading calculation
    prevFix.easting = pn.fix.easting;
    prevFix.northing = pn.fix.northing;
    prevDistFix = pn.fix;
}

// PHASE 6.0.42: Detect if GPS position jumped drastically
// Prevents gray screen and field corruption when GPS coordinates change significantly
// Use cases: SIM<->REAL mode switch, GPS module change, position correction after signal loss
bool FormGPS::detectGPSJump(double newLat, double newLon)
{
    // First GPS fix - not a jump
    if (m_lastKnownLatitude == 0 && m_lastKnownLongitude == 0) {
        m_lastKnownLatitude = newLat;
        m_lastKnownLongitude = newLon;
        return false;
    }

    // Calculate distance in kilometers using Haversine approximation
    // dLat: Latitude difference in km (111.32 km per degree latitude)
    // dLon: Longitude difference in km (adjusted by cosine of latitude for Earth curvature)
    double dLat = (newLat - m_lastKnownLatitude) * 111.32;
    double dLon = (newLon - m_lastKnownLongitude) * 111.32 * cos(newLat * 0.01745329);
    double distanceKm = sqrt(dLat*dLat + dLon*dLon);

    qDebug() << "GPS position check: distance from last known =" << distanceKm << "km";

    return (distanceKm > GPS_JUMP_THRESHOLD_KM);
}

// PHASE 6.0.42: Handle GPS jump - save field, regenerate OpenGL center, update position
// This function ensures clean transition when GPS coordinates change significantly:
// 1. Save and close current field (if open) to prevent coordinate corruption
// 2. Update latStart/lonStart with new GPS position (OpenGL reference point)
// 3. Reset GPS initialization flags for proper reinitialization
// 4. Update last known position for future jump detection
void FormGPS::handleGPSJump(double newLat, double newLon)
{
    qDebug() << "GPS JUMP DETECTED - regenerating OpenGL center";
    qDebug() << "Old position: lat=" << m_lastKnownLatitude << "lon=" << m_lastKnownLongitude;
    qDebug() << "New position: lat=" << newLat << "lon=" << newLon;

    // If field is open, save and close it to prevent coordinate corruption
    // Field data is tied to specific GPS coordinates (latStart/lonStart)
    // When GPS jumps, field coordinates no longer match real-world positions
    if (isJobStarted()) {
        qDebug() << "Field open during GPS jump - saving and closing";
        FileSaveEverythingBeforeClosingField(false);  // Save all field data (sections, boundary, contour, flags, tracks)
        JobClose();  // Close field properly (clears boundaries, sections, resets flags)
        qDebug() << "Field closed successfully";
    }

    // Update latStart/lonStart with new GPS position
    // These are the reference coordinates for WGS84->Local conversion
    // OpenGL rendering uses local meters (northing/easting) calculated from these
    setLatStart(newLat);
    setLonStart(newLon);
    pn.SetLocalMetersPerDegree(this);  // Recalculate meters per degree for new latitude

    // PHASE 6.0.42.1: Reset GPS initialization for reinitialization cycle
    // BUT set isFirstFixPositionSet = true because we just initialized latStart/lonStart above
    // This allows startCounter to increment immediately instead of wasting 1 cycle
    isGPSPositionInitialized = false;
    isFirstFixPositionSet = true;  // Position reference initialized above
    isFirstHeadingSet = false;
    startCounter = 0;

    // PHASE 6.0.42.1: Update pn structure with new GPS position
    // Ensures pn.latitude/longitude match the new reference point
    pn.latitude = newLat;
    pn.longitude = newLon;

    // CRITICAL: Convert new GPS position to local coordinates using new reference point
    // This ensures pn.fix.northing/easting are valid for the first UpdateFixPosition() call
    pn.ConvertWGS84ToLocal(newLat, newLon, pn.fix.northing, pn.fix.easting, this);

    // PHASE 6.0.42.1: Initialize raw GPS position for heading calculation
    // Symmetric to simulation mode initialization (formgps_sim.cpp:82-85)
    {
        QMutexLocker lock(&m_rawGpsPositionMutex);
        m_rawGpsPosition.easting = pn.fix.easting;
        m_rawGpsPosition.northing = pn.fix.northing;
    }

    // PHASE 6.0.42.1: Initialize prevFix for position tracking
    // Ensures first position update after jump has valid reference
    prevFix.easting = pn.fix.easting;
    prevFix.northing = pn.fix.northing;

    // PHASE 6.0.42: Reset guidance line distance offset
    // Old offset based on previous coordinate system is now invalid
    // Set to 32000 = "no guidance active" until new guidance line is calculated
    CVehicle::instance()->guidanceLineDistanceOff = 32000;
    CVehicle::instance()->guidanceLineSteerAngle = 0;

    // PHASE 6.0.42: Reset stepFixPts[] for heading calculation
    // Old position history from previous coordinate system is invalid
    // System will accumulate 3 new points and auto-calculate heading when speed > 1.5 km/h
    for (int i = 0; i < totalFixSteps; i++) {
        stepFixPts[i].isSet = 0;
    }

    // Update last known position for future jump detection
    m_lastKnownLatitude = newLat;
    m_lastKnownLongitude = newLon;
}

void FormGPS::tmrWatchdog_timeout()
{
    //TODO: replace all this with individual timers for cleaner

    // PHASE 6.0.40: Detect mode change and reset GPS state
    // Prevents gray screen when switching between sim and real modes
    static bool wasSimulatorOn = SettingsManager::instance()->menu_isSimulatorOn();
    bool isSimulatorOn = SettingsManager::instance()->menu_isSimulatorOn();

    if (wasSimulatorOn != isSimulatorOn) {
        // Mode changed - reset GPS state to prevent gray screen bug
        qDebug() << "Mode switch detected:" << (wasSimulatorOn ? "SIM to REAL" : "REAL to SIM");
        ResetGPSState(isSimulatorOn);
        wasSimulatorOn = isSimulatorOn;
    }

    if (! isSimulatorOn && timerSim.isActive()) {
        qDebug() << "Shutting down simulator.";
        timerSim.stop();
    } else if (isSimulatorOn && ! timerSim.isActive() ) {
        qDebug() << "Starting up simulator.";
        // Old initialization removed - now done in ResetGPSState()
        timerSim.start(100); //fire simulator every 100 ms = 10Hz
        gpsHz = 10; // sync gpsHz to sim rate
    }


    // This is done in QML
//    if ((uint)sentenceCounter++ > 20)
//    {
//        //TODO: ShowNoGPSWarning(;
//        return;
//    }
    // Rectangle Pattern: Use setter to properly increment and emit signal
    setSentenceCounter(sentenceCounter() + 1);


    if (tenSecondCounter++ >= 40)
    {
        tenSecondCounter = 0;
        tenSeconds++;
    }
    if (threeSecondCounter++ >= 12)
    {
        threeSecondCounter = 0;
        threeSeconds++;
    }
    if (oneSecondCounter++ >= 4)
    {
        oneSecondCounter = 0;
        oneSecond++;
    }
    if (oneHalfSecondCounter++ >= 2)
    {
        oneHalfSecondCounter = 0;
        oneHalfSecond++;
    }
    if (oneFifthSecondCounter++ >= 0)
    {
        oneFifthSecondCounter = 0;
        oneFifthSecond++;
    }

    ////////////////////////////////////////////// 10 second ///////////////////////////////////////////////////////
    //every 10 second update status
    if (tenSeconds != 0)
    {
        //reset the counter
        tenSeconds = 0;
    }
    /////////////////////////////////////////////////////////   333333333333333  ////////////////////////////////////////
    //every 3 second update status
    if (displayUpdateThreeSecondCounter != threeSeconds)
    {
        //reset the counter
        displayUpdateThreeSecondCounter = threeSeconds;

        //check to make sure the grid is big enough
        //worldGrid.checkZoomWorldGrid(pn.fix.northing, pn.fix.easting;

        //hide the NAv panel in 6  secs
        /* TODO:
        if (panelNavigation.Visible)
        {
            if (navPanelCounter-- < 2) panelNavigation.Visible = false;
        }

        if (panelNavigation.Visible)
            lblHz.Text = gpsHz.ToString("N1") + " ~ " + (frameTime.ToString("N1")) + " " + FixQuality;

        lblFix.Text = FixQuality + pn.age.ToString("N1";

        lblTime.Text = DateTime.Now.ToString("T";
        */

        //save nmea log file
        //TODO: if (isLogNMEA) FileSaveNMEA(;

        //update button lines numbers
        //TODO: UpdateGuidanceLineButtonNumbers(;

    }//end every 3 seconds

    //every second update all status ///////////////////////////   1 1 1 1 1 1 ////////////////////////////
    if (displayUpdateOneSecondCounter != oneSecond)
    {
        //reset the counter
        displayUpdateOneSecondCounter = oneSecond;

        //counter used for saving field in background
        minuteCounter++;
        tenMinuteCounter++;

        // PHASE 6.0.42.9: Auto-track timer increment (C# GUI.Designer.cs:275)
        // Enables automatic switching to closest track as tractor moves
        // Timer prevents rapid switching (max 1 switch/second when >= 1)
        track.autoTrack3SecTimer++;
    }

    //every half of a second update all status  ////////////////    0.5  0.5   0.5    0.5    /////////////////
    if (displayUpdateHalfSecondCounter != oneHalfSecond)
    {
        //reset the counter
        displayUpdateHalfSecondCounter = oneHalfSecond;

        isFlashOnOff = !isFlashOnOff;

        //the ratemap trigger
        worldGrid.isRateTrigger = true;

        //Make sure it is off when it should
        if ((!this->isContourBtnOn() && track.idx() == -1 && isBtnAutoSteerOn())
            ) onStopAutoSteer();

    } //end every 1/2 second

    //every fourth second update  ///////////////////////////   Fourth  ////////////////////////////
    {
        //reset the counter
        oneHalfSecondCounter++;
        oneSecondCounter++;
        yt.makeUTurnCounter++;

        secondsSinceStart = stopwatch.elapsed() / 1000.0;
    }
}

QString FormGPS::speedKPH() {
    double spd = CVehicle::instance()->avgSpeed;

    //convert to kph
    spd *= 0.1;

    return locale.toString(spd,'f',1);
}

QString FormGPS::speedMPH() {
    double spd = CVehicle::instance()->avgSpeed;

    //convert to mph
    spd *= 0.0621371;

    return locale.toString(spd,'f',1);
}

void FormGPS::SwapDirection() {
    if (!yt.isYouTurnTriggered)
    {
        yt.isYouTurnRight = ! yt.isYouTurnRight;
        yt.ResetCreatedYouTurn();
    }
    else if (this->isYouTurnBtnOn())
    {
        this->setIsYouTurnBtnOn(false);
    }
}


void FormGPS::JobClose()
{
    lock.lockForWrite();
    recPath.resumeState = 0;
    recPath.currentPositonIndex = 0;

    sbGrid.clear();

    //reset field offsets
    if (!isKeepOffsetsOn)
    {
        pn.fixOffset.easting = 0;
        pn.fixOffset.northing = 0;
    }

    //turn off headland
    this->setIsHeadlandOn(false); //this turns off the button

    recPath.recList.clear();
    recPath.StopDrivingRecordedPath();

    //make sure hydraulic lift is off
    p_239.pgn[p_239.hydLift] = 0;
    CVehicle::instance()->setIsHydLiftOn(false); //this turns off the button also - Qt 6.8

    //oglZoom.SendToBack(;

    //clean all the lines
    bnd.bndList.clear();
    //TODO: bnd.shpList.clear(;


    setIsJobStarted(false);

    //fix ManualOffOnAuto buttons
    this->setManualBtnState((int)btnStates::Off);

    //fix auto button
    this->setAutoBtnState((int)btnStates::Off);

    // ⚡ PHASE 6.0.20: Disable AutoSteer when job closes (safety + clean state)
    setIsBtnAutoSteerOn(false);

    /*
    btnZone1.BackColor = Color.Silver;
    btnZone2.BackColor = Color.Silver;
    btnZone3.BackColor = Color.Silver;
    btnZone4.BackColor = Color.Silver;
    btnZone5.BackColor = Color.Silver;
    btnZone6.BackColor = Color.Silver;
    btnZone7.BackColor = Color.Silver;
    btnZone8.BackColor = Color.Silver;

    btnZone1.Enabled = false;
    btnZone2.Enabled = false;
    btnZone3.Enabled = false;
    btnZone4.Enabled = false;
    btnZone5.Enabled = false;
    btnZone6.Enabled = false;
    btnZone7.Enabled = false;
    btnZone8.Enabled = false;

    btnSection1Man.Enabled = false;
    btnSection2Man.Enabled = false;
    btnSection3Man.Enabled = false;
    btnSection4Man.Enabled = false;
    btnSection5Man.Enabled = false;
    btnSection6Man.Enabled = false;
    btnSection7Man.Enabled = false;
    btnSection8Man.Enabled = false;
    btnSection9Man.Enabled = false;
    btnSection10Man.Enabled = false;
    btnSection11Man.Enabled = false;
    btnSection12Man.Enabled = false;
    btnSection13Man.Enabled = false;
    btnSection14Man.Enabled = false;
    btnSection15Man.Enabled = false;
    btnSection16Man.Enabled = false;

    btnSection1Man.BackColor = Color.Silver;
    btnSection2Man.BackColor = Color.Silver;
    btnSection3Man.BackColor = Color.Silver;
    btnSection4Man.BackColor = Color.Silver;
    btnSection5Man.BackColor = Color.Silver;
    btnSection6Man.BackColor = Color.Silver;
    btnSection7Man.BackColor = Color.Silver;
    btnSection8Man.BackColor = Color.Silver;
    btnSection9Man.BackColor = Color.Silver;
    btnSection10Man.BackColor = Color.Silver;
    btnSection11Man.BackColor = Color.Silver;
    btnSection12Man.BackColor = Color.Silver;
    btnSection13Man.BackColor = Color.Silver;
    btnSection14Man.BackColor = Color.Silver;
    btnSection15Man.BackColor = Color.Silver;
    btnSection16Man.BackColor = Color.Silver;
    */

    //clear the section lists
    for (int j = 0; j < triStrip.count(); j++)
    {
        //clean out the lists
        triStrip[j].patchList.clear();
        triStrip[j].triangleList.clear();
    }

    triStrip.clear();
    triStrip.append(CPatches());

    //invalidate all GPU patch list buffers. Must be destroyed
    //in the OpenGL context, so deferred to the next drawing
    //pass.
    patchesBufferDirty = true;

    //clear the flags
    flagPts.clear();

    //ABLine
    tram.tramList.clear();

    track.ResetCurveLine();

    //tracks
    track.gArr.clear();
    track.setIdx(-1);

    //clean up tram
    tram.displayMode = 0;
    tram.generateMode = 0;
    tram.tramBndInnerArr.clear();
    tram.tramBndOuterArr.clear();

    //clear out contour and Lists
    ct.ResetContour();
    this->setIsContourBtnOn(false); //turns off button in gui
    ct.isContourOn = false;

    //btnABDraw.Enabled = false;
    //btnCycleLines.Image = Properties.Resources.ABLineCycle;
    //btnCycleLines.Enabled = false;
    //btnCycleLinesBk.Image = Properties.Resources.ABLineCycleBk;
    //btnCycleLinesBk.Enabled = false;

    //AutoSteer
    //btnAutoSteer.Enabled = false;
    setIsBtnAutoSteerOn(false);

    //auto YouTurn shutdown
    this->setIsYouTurnBtnOn(false);

    yt.ResetYouTurn();

    //reset acre and distance counters
    setWorkedAreaTotal(0);

    //reset GUI areas
    fd.UpdateFieldBoundaryGUIAreas(bnd.bndList, mainWindow, this);

    displayFieldName = tr("None");

    recPath.recList.clear();
    recPath.shortestDubinsList.clear();
    recPath.shuttleDubinsList.clear();

    //FixPanelsAndMenus();
    camera.SetZoom();
    worldGrid.isGeoMap = false;
    worldGrid.isRateMap = false;

    //release Bing texture
    lock.unlock();
}

void FormGPS::JobNew()
{
    startCounter = 0;

    //btnSectionMasterManual.Enabled = true;
    this->setManualBtnState((int)btnStates::Off);
    //btnSectionMasterManual.Image = Properties.Resources.ManualOff;

    //btnSectionMasterAuto.Enabled = true;
    this->setAutoBtnState((int)btnStates::Off);
    //btnSectionMasterAuto.Image = Properties.Resources.SectionMasterOff;

    track.ABLine.abHeading = 0.00;

    camera.SetZoom();
    fileSaveCounter = 25;
    track.setIsAutoTrack(false);
    setIsJobStarted(true);

    // PHASE 6.0.29: Reset recorded path flags when opening field
    // Prevents steer from activating due to garbage flag values (formgps_position.cpp:800)
    setIsDrivingRecordedPath(false);
    recPath.isFollowingDubinsToPath = false;
    recPath.isFollowingRecPath = false;
    recPath.isFollowingDubinsHome = false;

    // PHASE 6.0.30: Auto-enable recording when job starts
    // Vehicle trail (yellow line) needs isRecordOn=true to populate recList (formgps_position.cpp:1821)
    // Phase 6.0.29 initialized isRecordOn=false in constructor, which emptied the trail after JobNew()
    recPath.isRecordOn = true;
    patchesBufferDirty = true;
}

void FormGPS::FileSaveEverythingBeforeClosingField(bool saveVehicle)
{
    qDebug() << "shutting down, saving field items.";

    if (! isJobStarted()) return;

    qDebug() << "Test3";
    lock.lockForWrite();
    //turn off contour line if on
    if (ct.isContourOn) ct.StopContourLine(contourSaveList);

    //turn off all the sections
    for (int j = 0; j < tool.numOfSections; j++)
    {
        tool.section[j].sectionOnOffCycle = false;
        tool.section[j].sectionOffRequest = false;
    }

    //turn off patching
    for (int j = 0; j < triStrip.count(); j++)
    {
        if (triStrip[j].isDrawing) triStrip[j].TurnMappingOff(tool, fd, mainWindow, this);
    }
    lock.unlock();
    qDebug() << "Test4";

    //FileSaveHeadland(;
    qDebug() << "Starting FileSaveBoundary()";
    FileSaveBoundary();
    qDebug() << "Starting FileSaveSections()";
    FileSaveSections();
    qDebug() << "Starting FileSaveContour()";
    FileSaveContour();
    qDebug() << "Starting FileSaveTracks()";
    FileSaveTracks();
    qDebug() << "Starting FileSaveFlags()";
    FileSaveFlags();
    qDebug() << "Starting ExportFieldAs_KML()";
    ExportFieldAs_KML();
    qDebug() << "All file operations completed";
    //ExportFieldAs_ISOXMLv3()
    //ExportFieldAs_ISOXMLv4()

    // Save vehicle settings AFTER all field operations complete (conditional)
    // Include applicationClosing property in save decision (Qt 6.8 Rectangle Pattern)
    bool shouldSaveVehicle = saveVehicle || applicationClosing();
    qDebug() << "Before vehicle_saveas check, saveVehicle=" << saveVehicle << "applicationClosing=" << applicationClosing() << "shouldSaveVehicle=" << shouldSaveVehicle;
    if(shouldSaveVehicle && SettingsManager::instance()->vehicle_vehicleName() != "Default Vehicle") {
        QString vehicleName = SettingsManager::instance()->vehicle_vehicleName();
        qDebug() << "Scheduling async vehicle_saveas():" << vehicleName;

        // ASYNC SOLUTION: Defer vehicle_saveas to avoid mutex deadlock during field close
        QTimer::singleShot(100, this, [this, vehicleName]() {
            qDebug() << "Executing async vehicle_saveas():" << vehicleName;
            vehicle_saveas(vehicleName);
            qDebug() << "Async vehicle_saveas() completed";
        });
    } else {
        qDebug() << "Skipping vehicle_saveas (saveVehicle=" << saveVehicle << "applicationClosing=" << applicationClosing() << "shouldSaveVehicle=" << shouldSaveVehicle << ")";
    }

    qDebug() << "Before field cleanup";
    //property_setF_CurrentDir = tr("None";
    //currentFieldDirectory = (QString)property_setF_CurrentDir;
    displayFieldName = tr("None");

    qDebug() << "Before JobClose()";
    JobClose();
    qDebug() << "JobClose() completed";
    //Text = "AgOpenGPS";
    qDebug() << "Test5";
}

// AgIO Service Setup Methods
void FormGPS::setupAgIOService()
{
    qDebug() << "Setting up AgIO service (main thread)...";

    // Phase 6.0.21: Get AgIOService singleton instance for signal connection
    m_agioService = AgIOService::instance();

    qDebug() << "AgIOService singleton accessed - ready for parsedDataReady signal connection";
}

void FormGPS::connectToAgIOFactoryInstance()
{
    qDebug() << "ðŸ”— Connecting to AgIOService factory instance...";
    
    // Get the factory-created singleton instance
    m_agioService = AgIOService::instance();
    
    if (m_agioService) {
        qDebug() << "âœ… Connected to AgIOService singleton instance";
        
        // Now connect the Phase 4.2 pipeline: AgIOService â†’ pn â†’ vehicle â†’ OpenGL
        connectFormLoopToAgIOService();
        
        qDebug() << "ðŸ”— Phase 4.2: AgIOService â†’ pn â†’ vehicle pipeline established";
    } else {
        qDebug() << "âŒ ERROR: AgIOService singleton not found";
    }
}

void FormGPS::testAgIOConfiguration()
{
    qDebug() << "\n=================================";
    qDebug() << "ðŸ“‹ AgIO Configuration Test";
    qDebug() << "=================================";
    
    QSettings settings("QtAgOpenGPS", "QtAgOpenGPS");
    qDebug() << "ðŸ“ Settings file:" << settings.fileName();
    
    // Display NTRIP settings
    qDebug() << "\nðŸŒ NTRIP Configuration:";
    qDebug() << "  URL:" << settings.value("comm/ntripURL", "").toString();
    qDebug() << "  Mount:" << settings.value("comm/ntripMount", "").toString();
    qDebug() << "  Port:" << settings.value("comm/ntripCasterPort", 2101).toInt();
    qDebug() << "  Enabled:" << settings.value("comm/ntripIsOn", false).toBool();
    qDebug() << "  User:" << (settings.value("comm/ntripUserName", "").toString().isEmpty() ? "none" : "configured");
    
    // Display UDP settings
    qDebug() << "\nðŸ“¡ UDP Configuration:";
    int ip1 = settings.value("comm/udpIP1", 192).toInt();
    int ip2 = settings.value("comm/udpIP2", 168).toInt();
    int ip3 = settings.value("comm/udpIP3", 5).toInt();
    qDebug() << "  Subnet:" << QString("%1.%2.%3.xxx").arg(ip1).arg(ip2).arg(ip3);
    qDebug() << "  Broadcast:" << QString("%1.%2.%3.255").arg(ip1).arg(ip2).arg(ip3);
    qDebug() << "  Listen Port:" << settings.value("comm/udpListenPort", 9999).toInt();
    qDebug() << "  Send Port:" << settings.value("comm/udpSendPort", 8888).toInt();
    
    qDebug() << "\nðŸ” Expected Data Sources:";
    qDebug() << "  1. GPS via UDP on port 9999 (NMEA sentences)";
    qDebug() << "  2. GPS via Serial port (if configured)";
    qDebug() << "  3. NTRIP corrections from" << settings.value("comm/ntripURL", "").toString();
    qDebug() << "  4. AgOpenGPS modules on" << QString("%1.%2.%3.255").arg(ip1).arg(ip2).arg(ip3);
    qDebug() << "=================================\n";
}

void FormGPS::connectFormLoopToAgIOService()
{
    qDebug() << "ðŸ”— Phase 4.2: Connecting AgIOService â†’ pn â†’ vehicle ...";
    
    if (!m_agioService) {
        qDebug() << "âŒ Cannot connect: AgIOService is null";
        return;
    }
    
    // PHASE 4.2: Direct connection AgIOService â†’ pn â†’ vehicle
    // This replaces FormLoop progressively as per architecture document
    
    // ✅ RECTANGLE PATTERN PURE: Direct access via updateGPSData() method
    // GPS data automatically synchronized via property bindings and main timer
    // No connect() needed - updateGPSData() called directly when needed
    
    // Phase 6.0.21: IMU initialization removed - GPS/IMU data now comes via gpsDataReceived signal
    // Data flow: AgIOService broadcasts via signal -> FormGPS stores -> ahrs updated in UpdateFixPosition()
    // ahrs.imuRoll, ahrs.imuPitch, ahrs.imuHeading initialized from signal data

    qDebug() << "OK AgIOService -> FormGPS signal pipeline established";
    qDebug() << "  Data flow: AgIOService (broadcast) -> FormGPS (storage) -> vehicle -> OpenGL";
    qDebug() << "  Phase 6.0.21: GPS/IMU data via gpsDataReceived signal";
}

void FormGPS::cleanupAgIOService()
{
    qDebug() << "ðŸ”§ Cleaning up AgIO service...";

    if (m_agioService) {
        m_agioService->shutdown();
        // Note: Don't delete m_agioService as it's managed by QML singleton system
        m_agioService = nullptr;
        qDebug() << "âœ… AgIO service cleaned up";
    }
}

// Qt 6.8: All complex property binding methods removed
// Using simple objectCreated signal instead

