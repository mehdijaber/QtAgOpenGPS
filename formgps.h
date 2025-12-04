#ifndef FORMGPS_H
#define FORMGPS_H

#include <QMainWindow>
#include <QScopedPointer>
#include <memory> // C++17 smart pointers
#include <QProperty> // Qt 6.8 QProperty + BINDABLE
#include <QBindable> // Qt 6.8 QBindable for automatic change tracking
#include <QtQuick/QQuickItem>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QUdpSocket>
#include <QElapsedTimer>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QOpenGLFramebufferObject>
#include <QOpenGLBuffer>
#include <QQmlApplicationEngine>
#include <QTranslator>
//#include <QSerialPort>
#include "common.h"
#include "vecfix2fix.h"
#include "vec2.h"
#include "vec3.h"
#include "cflag.h"
#include "cmodulecomm.h"
#include "ccamera.h"
#include "btnenum.h"

#include "cworldgrid.h"
#include "cnmea.h"
#include "cvehicle.h"
#include "ctool.h"
#include "agioservice.h"
#include "pgnparser.h"  // Phase 6.0.21: For ParsedData struct
#include "cboundary.h"
#include "cabline.h"
#include "ctram.h"
#include "ccontour.h"
#include "cabcurve.h"
#include "cyouturn.h"
#include "cfielddata.h"
#include "csim.h"
#include "cahrs.h"
#include "crecordedpath.h"
#include "cguidance.h"
#include "cheadline.h"
#include "cpgn.h"
#include "ctrack.h"

#include "formheadland.h"
#include "formheadache.h"

#include <QVector>
#include <QDateTime>
#include <QDebug>

//forward declare classes referred to below, to break circular
//references in the code
class QOpenGLShaderProgram;
class AOGRendererInSG;
class QQuickCloseEvent;
class QVector3D;

struct PatchBuffer {
    QOpenGLBuffer patchBuffer;
    int length;
};

struct PatchInBuffer {
    int which;
    int offset;
    int length;
};


class FormGPS : public QQmlApplicationEngine
{
    Q_OBJECT

    // ===== Q_PROPERTY MIGRATION - OPTION A =====
    // 67 properties organized in groups for 50Hz optimization

    // === Core Application State (2 properties) - Critical for basic operations - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(bool isJobStarted READ isJobStarted WRITE setIsJobStarted
               NOTIFY isJobStartedChanged BINDABLE bindableIsJobStarted)
    Q_PROPERTY(bool isBtnAutoSteerOn READ isBtnAutoSteerOn WRITE setIsBtnAutoSteerOn
               NOTIFY isBtnAutoSteerOnChanged BINDABLE bindableIsBtnAutoSteerOn)

    // === CRITICAL: applicationClosing property for save_everything fix ===
    Q_PROPERTY(bool applicationClosing READ applicationClosing WRITE setApplicationClosing
               NOTIFY applicationClosingChanged BINDABLE bindableApplicationClosing)

    // === Position GPS (6 properties) - Critical for navigation - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(double latitude READ latitude WRITE setLatitude
               NOTIFY latitudeChanged BINDABLE bindableLatitude)
    Q_PROPERTY(double longitude READ longitude WRITE setLongitude
               NOTIFY longitudeChanged BINDABLE bindableLongitude)
    Q_PROPERTY(double altitude READ altitude WRITE setAltitude
               NOTIFY altitudeChanged BINDABLE bindableAltitude)
    Q_PROPERTY(double easting READ easting WRITE setEasting
               NOTIFY eastingChanged BINDABLE bindableEasting)
    Q_PROPERTY(double northing READ northing WRITE setNorthing
               NOTIFY northingChanged BINDABLE bindableNorthing)
    Q_PROPERTY(double heading READ heading WRITE setHeading
               NOTIFY headingChanged BINDABLE bindableHeading)
    Q_PROPERTY(QVariantList sectionButtonState READ sectionButtonState WRITE setSectionButtonState
               NOTIFY sectionButtonStateChanged BINDABLE bindableSectionButtonState)

    // Qt 6.8: Removed complex property binding - using objectCreated signal instead

    // === Vehicle State (6 properties) - Critical for guidance - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(double speedKph READ speedKph WRITE setSpeedKph
               NOTIFY speedKphChanged BINDABLE bindableSpeedKph)
    Q_PROPERTY(double fusedHeading READ fusedHeading WRITE setFusedHeading
               NOTIFY fusedHeadingChanged BINDABLE bindableFusedHeading)
    Q_PROPERTY(double toolEasting READ toolEasting WRITE setToolEasting
               NOTIFY toolEastingChanged BINDABLE bindableToolEasting)
    Q_PROPERTY(double toolNorthing READ toolNorthing WRITE setToolNorthing
               NOTIFY toolNorthingChanged BINDABLE bindableToolNorthing)
    Q_PROPERTY(double toolHeading READ toolHeading WRITE setToolHeading
               NOTIFY toolHeadingChanged BINDABLE bindableToolHeading)
    Q_PROPERTY(double offlineDistance READ offlineDistance WRITE setOfflineDistance
               NOTIFY offlineDistanceChanged BINDABLE bindableOfflineDistance)
    Q_PROPERTY(double avgPivDistance READ avgPivDistance WRITE setAvgPivDistance
               NOTIFY avgPivDistanceChanged BINDABLE bindableAvgPivDistance)

    // === Steering Control (6 properties) - Critical for autosteer - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(double steerAngleActual READ steerAngleActual WRITE setSteerAngleActual
               NOTIFY steerAngleActualChanged BINDABLE bindableSteerAngleActual)
    Q_PROPERTY(double steerAngleSet READ steerAngleSet WRITE setSteerAngleSet
               NOTIFY steerAngleSetChanged BINDABLE bindableSteerAngleSet)
    Q_PROPERTY(int lblPWMDisplay READ lblPWMDisplay WRITE setLblPWMDisplay
               NOTIFY lblPWMDisplayChanged BINDABLE bindableLblPWMDisplay)
    Q_PROPERTY(double calcSteerAngleInner READ calcSteerAngleInner WRITE setCalcSteerAngleInner
               NOTIFY calcSteerAngleInnerChanged BINDABLE bindableCalcSteerAngleInner)
    Q_PROPERTY(double calcSteerAngleOuter READ calcSteerAngleOuter WRITE setCalcSteerAngleOuter
               NOTIFY calcSteerAngleOuterChanged BINDABLE bindableCalcSteerAngleOuter)
    Q_PROPERTY(double diameter READ diameter WRITE setDiameter
               NOTIFY diameterChanged BINDABLE bindableDiameter)

    // === IMU Data (5 properties) - Important for stability - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(double imuRoll READ imuRoll WRITE setImuRoll
               NOTIFY imuRollChanged BINDABLE bindableImuRoll)
    Q_PROPERTY(double imuPitch READ imuPitch WRITE setImuPitch
               NOTIFY imuPitchChanged BINDABLE bindableImuPitch)
    Q_PROPERTY(double imuHeading READ imuHeading WRITE setImuHeading
               NOTIFY imuHeadingChanged BINDABLE bindableImuHeading)
    Q_PROPERTY(double imuRollDegrees READ imuRollDegrees WRITE setImuRollDegrees
               NOTIFY imuRollDegreesChanged BINDABLE bindableImuRollDegrees)
    Q_PROPERTY(double imuAngVel READ imuAngVel WRITE setImuAngVel
               NOTIFY imuAngVelChanged BINDABLE bindableImuAngVel)
    Q_PROPERTY(double yawRate READ yawRate WRITE setYawRate
               NOTIFY yawRateChanged BINDABLE bindableYawRate)

    // === GPS Status (6 properties) - Important for monitoring - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(double hdop READ hdop WRITE setHdop
               NOTIFY hdopChanged BINDABLE bindableHdop)
    Q_PROPERTY(double age READ age WRITE setAge
               NOTIFY ageChanged BINDABLE bindableAge)
    Q_PROPERTY(int fixQuality READ fixQuality WRITE setFixQuality
               NOTIFY fixQualityChanged BINDABLE bindableFixQuality)
    Q_PROPERTY(int satellitesTracked READ satellitesTracked WRITE setSatellitesTracked
               NOTIFY satellitesTrackedChanged BINDABLE bindableSatellitesTracked)
    Q_PROPERTY(double hz READ hz WRITE setHz
               NOTIFY hzChanged BINDABLE bindableHz)
    Q_PROPERTY(double rawHz READ rawHz WRITE setRawHz
               NOTIFY rawHzChanged BINDABLE bindableRawHz)
    Q_PROPERTY(double frameTime READ frameTime WRITE setFrameTime
               NOTIFY frameTimeChanged BINDABLE bindableFrameTime)

    // === Blockage Sensors (8 properties) - Monitoring - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(double blockage_avg READ blockage_avg WRITE setBlockage_avg
               NOTIFY blockage_avgChanged BINDABLE bindableBlockage_avg)
    Q_PROPERTY(double blockage_min1 READ blockage_min1 WRITE setBlockage_min1
               NOTIFY blockage_min1Changed BINDABLE bindableBlockage_min1)
    Q_PROPERTY(double blockage_min2 READ blockage_min2 WRITE setBlockage_min2
               NOTIFY blockage_min2Changed BINDABLE bindableBlockage_min2)
    Q_PROPERTY(double blockage_max READ blockage_max WRITE setBlockage_max
               NOTIFY blockage_maxChanged BINDABLE bindableBlockage_max)
    Q_PROPERTY(int blockage_min1_i READ blockage_min1_i WRITE setBlockage_min1_i
               NOTIFY blockage_min1_iChanged BINDABLE bindableBlockage_min1_i)
    Q_PROPERTY(int blockage_min2_i READ blockage_min2_i WRITE setBlockage_min2_i
               NOTIFY blockage_min2_iChanged BINDABLE bindableBlockage_min2_i)
    Q_PROPERTY(int blockage_max_i READ blockage_max_i WRITE setBlockage_max_i
               NOTIFY blockage_max_iChanged BINDABLE bindableBlockage_max_i)
    Q_PROPERTY(bool blockage_blocked READ blockage_blocked WRITE setBlockage_blocked
               NOTIFY blockage_blockedChanged BINDABLE bindableBlockage_blocked)

    // === Navigation (7 properties) - Important for guidance - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(double distancePivotToTurnLine READ distancePivotToTurnLine WRITE setDistancePivotToTurnLine
               NOTIFY distancePivotToTurnLineChanged BINDABLE bindableDistancePivotToTurnLine)
    Q_PROPERTY(bool isYouTurnRight READ isYouTurnRight WRITE setIsYouTurnRight
               NOTIFY isYouTurnRightChanged BINDABLE bindableIsYouTurnRight)
    Q_PROPERTY(bool isYouTurnTriggered READ isYouTurnTriggered WRITE setIsYouTurnTriggered
               NOTIFY isYouTurnTriggeredChanged BINDABLE bindableIsYouTurnTriggered)
    Q_PROPERTY(int current_trackNum READ current_trackNum WRITE setCurrent_trackNum
               NOTIFY current_trackNumChanged BINDABLE bindableCurrent_trackNum)
    Q_PROPERTY(int track_idx READ track_idx WRITE setTrack_idx
               NOTIFY track_idxChanged BINDABLE bindableTrack_idx)
    Q_PROPERTY(double lblmodeActualXTE READ lblmodeActualXTE WRITE setLblmodeActualXTE
               NOTIFY lblmodeActualXTEChanged BINDABLE bindableLblmodeActualXTE)
    Q_PROPERTY(double lblmodeActualHeadingError READ lblmodeActualHeadingError WRITE setLblmodeActualHeadingError
               NOTIFY lblmodeActualHeadingErrorChanged BINDABLE bindableLblmodeActualHeadingError)

    // === Tool Position (2 properties) - Display - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(double toolLatitude READ toolLatitude WRITE setToolLatitude
               NOTIFY toolLatitudeChanged BINDABLE bindableToolLatitude)
    Q_PROPERTY(double toolLongitude READ toolLongitude WRITE setToolLongitude
               NOTIFY toolLongitudeChanged BINDABLE bindableToolLongitude)

    // === Wizard/Calibration (4 properties) - Special - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(int sampleCount READ sampleCount WRITE setSampleCount
               NOTIFY sampleCountChanged BINDABLE bindableSampleCount)
    Q_PROPERTY(double confidenceLevel READ confidenceLevel WRITE setConfidenceLevel
               NOTIFY confidenceLevelChanged BINDABLE bindableConfidenceLevel)
    Q_PROPERTY(bool hasValidRecommendation READ hasValidRecommendation WRITE setHasValidRecommendation
               NOTIFY hasValidRecommendationChanged BINDABLE bindableHasValidRecommendation)
    Q_PROPERTY(bool startSA READ startSA WRITE setStartSA
               NOTIFY startSAChanged BINDABLE bindableStartSA)

    // === Visual Geometry (2 properties) - OpenGL - Qt 6.8 Rectangle Pattern ===
    Q_PROPERTY(QVariant vehicle_xy READ vehicle_xy WRITE setVehicle_xy
               NOTIFY vehicle_xyChanged BINDABLE bindableVehicle_xy)
    Q_PROPERTY(QVariant vehicle_bounding_box READ vehicle_bounding_box WRITE setVehicle_bounding_box
               NOTIFY vehicle_bounding_boxChanged BINDABLE bindableVehicle_bounding_box)

    // === Misc Status (2 properties) - Status - Qt 6.8 QProperty + BINDABLE ===
    Q_PROPERTY(bool steerSwitchHigh READ steerSwitchHigh WRITE setSteerSwitchHigh
               NOTIFY steerSwitchHighChanged BINDABLE bindableSteerSwitchHigh)
    Q_PROPERTY(bool imuCorrected READ imuCorrected WRITE setImuCorrected
               NOTIFY imuCorrectedChanged BINDABLE bindableImuCorrected)
    Q_PROPERTY(QString lblCalcSteerAngleInner READ lblCalcSteerAngleInner WRITE setLblCalcSteerAngleInner
               NOTIFY lblCalcSteerAngleInnerChanged BINDABLE bindableLblCalcSteerAngleInner)
    Q_PROPERTY(QString lblDiameter READ lblDiameter WRITE setLblDiameter
               NOTIFY lblDiameterChanged BINDABLE bindableLblDiameter)
    Q_PROPERTY(int droppedSentences READ droppedSentences WRITE setDroppedSentences
               NOTIFY droppedSentencesChanged BINDABLE bindableDroppedSentences)

    // === Field Data (11 properties) - Qt 6.8 QProperty + BINDABLE ===
    Q_PROPERTY(double areaOuterBoundary READ areaOuterBoundary WRITE setAreaOuterBoundary
               NOTIFY areaOuterBoundaryChanged BINDABLE bindableAreaOuterBoundary)
    Q_PROPERTY(double areaBoundaryOuterLessInner READ areaBoundaryOuterLessInner WRITE setAreaBoundaryOuterLessInner
               NOTIFY areaBoundaryOuterLessInnerChanged BINDABLE bindableAreaBoundaryOuterLessInner)
    Q_PROPERTY(double workedAreaTotal READ workedAreaTotal WRITE setWorkedAreaTotal
               NOTIFY workedAreaTotalChanged BINDABLE bindableWorkedAreaTotal)
    Q_PROPERTY(double workedAreaTotalUser READ workedAreaTotalUser WRITE setWorkedAreaTotalUser
               NOTIFY workedAreaTotalUserChanged BINDABLE bindableWorkedAreaTotalUser)
    Q_PROPERTY(double distanceUser READ distanceUser WRITE setDistanceUser
               NOTIFY distanceUserChanged BINDABLE bindableDistanceUser)
    Q_PROPERTY(double actualAreaCovered READ actualAreaCovered WRITE setActualAreaCovered
               NOTIFY actualAreaCoveredChanged BINDABLE bindableActualAreaCovered)
    Q_PROPERTY(double userSquareMetersAlarm READ userSquareMetersAlarm WRITE setUserSquareMetersAlarm
               NOTIFY userSquareMetersAlarmChanged BINDABLE bindableUserSquareMetersAlarm)
    Q_PROPERTY(bool isContourBtnOn READ isContourBtnOn WRITE setIsContourBtnOn
               NOTIFY isContourBtnOnChanged BINDABLE bindableIsContourBtnOn)
    Q_PROPERTY(bool isYouTurnBtnOn READ isYouTurnBtnOn WRITE setIsYouTurnBtnOn
               NOTIFY isYouTurnBtnOnChanged BINDABLE bindableIsYouTurnBtnOn)
    Q_PROPERTY(int sensorData READ sensorData WRITE setSensorData
               NOTIFY sensorDataChanged BINDABLE bindableSensorData)
    Q_PROPERTY(bool btnIsContourLocked READ btnIsContourLocked WRITE setBtnIsContourLocked
               NOTIFY btnIsContourLockedChanged BINDABLE bindableBtnIsContourLocked)

    // GPS/NMEA Coordinates - Phase 6.0.4.2
    // Phase 6.0.20 Task 24 Step 3.5: Read-only Q_PROPERTY (QML cannot modify field origin)
    Q_PROPERTY(double latStart READ latStart
               NOTIFY latStartChanged BINDABLE bindableLatStart)
    Q_PROPERTY(double lonStart READ lonStart
               NOTIFY lonStartChanged BINDABLE bindableLonStart)

    // mPerDegreeLat: No Q_PROPERTY needed - C++ only (CNMEA/AgIOService read via getter)

    // NMEA Processing - Phase 6.0.4.2 - Qt 6.8 QProperty + BINDABLE
    Q_PROPERTY(uint sentenceCounter READ sentenceCounter WRITE setSentenceCounter
               NOTIFY sentenceCounterChanged BINDABLE bindableSentenceCounter)

    // GPS/IMU Heading - Phase 6.0.20 Task 24 Step 2 - Qt 6.8 QProperty + BINDABLE
    Q_PROPERTY(double gpsHeading READ gpsHeading WRITE setGpsHeading
               NOTIFY gpsHeadingChanged BINDABLE bindableGpsHeading)
    Q_PROPERTY(bool isReverseWithIMU READ isReverseWithIMU WRITE setIsReverseWithIMU
               NOTIFY isReverseWithIMUChanged BINDABLE bindableIsReverseWithIMU)

    // Module Connection - Phase 6.0.20 Task 24 Step 3.2 - Qt 6.8 QProperty + BINDABLE
    Q_PROPERTY(int steerModuleConnectedCounter READ steerModuleConnectedCounter WRITE setSteerModuleConnectedCounter
               NOTIFY steerModuleConnectedCounterChanged BINDABLE bindableSteerModuleConnectedCounter)

    // Button States - Phase 6.0.4.2 - Qt 6.8 QProperty + BINDABLE
    Q_PROPERTY(int autoBtnState READ autoBtnState WRITE setAutoBtnState
               NOTIFY autoBtnStateChanged BINDABLE bindableAutoBtnState)
    Q_PROPERTY(int manualBtnState READ manualBtnState WRITE setManualBtnState
               NOTIFY manualBtnStateChanged BINDABLE bindableManualBtnState)
    Q_PROPERTY(bool autoTrackBtnState READ autoTrackBtnState WRITE setAutoTrackBtnState
               NOTIFY autoTrackBtnStateChanged BINDABLE bindableAutoTrackBtnState)
    Q_PROPERTY(bool autoYouturnBtnState READ autoYouturnBtnState WRITE setAutoYouturnBtnState
               NOTIFY autoYouturnBtnStateChanged BINDABLE bindableAutoYouturnBtnState)

    // Job Control - Phase 6.0.4.2 - Qt 6.8 QProperty + BINDABLE
    Q_PROPERTY(bool isPatchesChangingColor READ isPatchesChangingColor WRITE setIsPatchesChangingColor
               NOTIFY isPatchesChangingColorChanged BINDABLE bindableIsPatchesChangingColor)

    // Boundary Interface - Phase 6.0.4.2 - Qt 6.8 QProperty + BINDABLE
    Q_PROPERTY(bool isOutOfBounds READ isOutOfBounds WRITE setIsOutOfBounds
               NOTIFY isOutOfBoundsChanged BINDABLE bindableIsOutOfBounds)
    Q_PROPERTY(bool isHeadlandOn READ isHeadlandOn WRITE setIsHeadlandOn
               NOTIFY isHeadlandOnChanged BINDABLE bindableIsHeadlandOn)
    Q_PROPERTY(double createBndOffset READ createBndOffset WRITE setCreateBndOffset
               NOTIFY createBndOffsetChanged BINDABLE bindableCreateBndOffset)
    Q_PROPERTY(bool isDrawRightSide READ isDrawRightSide WRITE setIsDrawRightSide
               NOTIFY isDrawRightSideChanged BINDABLE bindableIsDrawRightSide)

    // RecordedPath Interface - Phase 6.0.4.2 - Qt 6.8 QProperty + BINDABLE
    Q_PROPERTY(bool isDrivingRecordedPath READ isDrivingRecordedPath WRITE setIsDrivingRecordedPath
               NOTIFY isDrivingRecordedPathChanged BINDABLE bindableIsDrivingRecordedPath)
    Q_PROPERTY(QString recordedPathName READ recordedPathName WRITE setRecordedPathName
               NOTIFY recordedPathNameChanged BINDABLE bindableRecordedPathName)

    // Boundary State - Phase 6.0.20 - Qt 6.8 BINDABLE
    Q_PROPERTY(bool boundaryIsRecording READ boundaryIsRecording WRITE setBoundaryIsRecording
               NOTIFY boundaryIsRecordingChanged BINDABLE bindableBoundaryIsRecording)
    Q_PROPERTY(double boundaryArea READ boundaryArea WRITE setBoundaryArea
               NOTIFY boundaryAreaChanged BINDABLE bindableBoundaryArea)
    Q_PROPERTY(int boundaryPointCount READ boundaryPointCount WRITE setBoundaryPointCount
               NOTIFY boundaryPointCountChanged BINDABLE bindableBoundaryPointCount)

public:
    explicit FormGPS(QWidget *parent = 0);
    ~FormGPS();

    // ===== Q_PROPERTY GETTERS, SETTERS AND BINDABLES =====
    // Manual declarations for all Rectangle Pattern properties

    // Application State
    bool isJobStarted() const;
    void setIsJobStarted(bool value);
    QBindable<bool> bindableIsJobStarted();

    bool isBtnAutoSteerOn() const;
    void setIsBtnAutoSteerOn(bool value);
    QBindable<bool> bindableIsBtnAutoSteerOn();

    bool applicationClosing() const;
    void setApplicationClosing(bool value);
    QBindable<bool> bindableApplicationClosing();

    // Position GPS
    double latitude() const;
    void setLatitude(double value);
    QBindable<double> bindableLatitude();

    double longitude() const;
    void setLongitude(double value);
    QBindable<double> bindableLongitude();

    double altitude() const;
    void setAltitude(double value);
    QBindable<double> bindableAltitude();

    double easting() const;
    void setEasting(double value);
    QBindable<double> bindableEasting();

    double northing() const;
    void setNorthing(double value);
    QBindable<double> bindableNorthing();

    double heading() const;
    void setHeading(double value);
    QBindable<double> bindableHeading();

    QVariantList sectionButtonState() const;
    void setSectionButtonState(const QVariantList& value);
    QBindable<QVariantList> bindableSectionButtonState();

    // Vehicle State
    double speedKph() const;
    void setSpeedKph(double value);
    QBindable<double> bindableSpeedKph();

    double fusedHeading() const;
    void setFusedHeading(double value);
    QBindable<double> bindableFusedHeading();

    double toolEasting() const;
    void setToolEasting(double value);
    QBindable<double> bindableToolEasting();

    double toolNorthing() const;
    void setToolNorthing(double value);
    QBindable<double> bindableToolNorthing();

    double toolHeading() const;
    void setToolHeading(double value);
    QBindable<double> bindableToolHeading();

    double offlineDistance() const;
    void setOfflineDistance(double value);
    QBindable<double> bindableOfflineDistance();

    // Steering Control
    double steerAngleActual() const;
    void setSteerAngleActual(double value);
    QBindable<double> bindableSteerAngleActual();

    double steerAngleSet() const;
    void setSteerAngleSet(double value);
    QBindable<double> bindableSteerAngleSet();

    int lblPWMDisplay() const;
    void setLblPWMDisplay(int value);
    QBindable<int> bindableLblPWMDisplay();

    double calcSteerAngleInner() const;
    void setCalcSteerAngleInner(double value);
    QBindable<double> bindableCalcSteerAngleInner();

    double calcSteerAngleOuter() const;
    void setCalcSteerAngleOuter(double value);
    QBindable<double> bindableCalcSteerAngleOuter();

    double diameter() const;
    void setDiameter(double value);
    QBindable<double> bindableDiameter();

    // IMU Data
    double imuRoll() const;
    void setImuRoll(double value);
    QBindable<double> bindableImuRoll();

    double imuPitch() const;
    void setImuPitch(double value);
    QBindable<double> bindableImuPitch();

    double imuHeading() const;
    void setImuHeading(double value);
    QBindable<double> bindableImuHeading();

    double imuRollDegrees() const;
    void setImuRollDegrees(double value);
    QBindable<double> bindableImuRollDegrees();

    double imuAngVel() const;
    void setImuAngVel(double value);
    QBindable<double> bindableImuAngVel();

    double yawRate() const;
    void setYawRate(double value);
    QBindable<double> bindableYawRate();

    // GPS Status
    double hdop() const;
    void setHdop(double value);
    QBindable<double> bindableHdop();

    double age() const;
    void setAge(double value);
    QBindable<double> bindableAge();

    int fixQuality() const;
    void setFixQuality(int value);
    QBindable<int> bindableFixQuality();

    int satellitesTracked() const;
    void setSatellitesTracked(int value);
    QBindable<int> bindableSatellitesTracked();

    double hz() const;
    void setHz(double value);
    QBindable<double> bindableHz();

    double rawHz() const;
    void setRawHz(double value);
    QBindable<double> bindableRawHz();

    // Blockage Sensors
    double blockage_avg() const;
    void setBlockage_avg(double value);
    QBindable<double> bindableBlockage_avg();

    double blockage_min1() const;
    void setBlockage_min1(double value);
    QBindable<double> bindableBlockage_min1();

    double blockage_min2() const;
    void setBlockage_min2(double value);
    QBindable<double> bindableBlockage_min2();

    double blockage_max() const;
    void setBlockage_max(double value);
    QBindable<double> bindableBlockage_max();

    int blockage_min1_i() const;
    void setBlockage_min1_i(int value);
    QBindable<int> bindableBlockage_min1_i();

    int blockage_min2_i() const;
    void setBlockage_min2_i(int value);
    QBindable<int> bindableBlockage_min2_i();

    int blockage_max_i() const;
    void setBlockage_max_i(int value);
    QBindable<int> bindableBlockage_max_i();

    bool blockage_blocked() const;
    void setBlockage_blocked(bool value);
    QBindable<bool> bindableBlockage_blocked();

    double avgPivDistance() const;
    void setAvgPivDistance(double value);
    QBindable<double> bindableAvgPivDistance();

    double frameTime() const;
    void setFrameTime(double value);
    QBindable<double> bindableFrameTime();

    // Navigation
    double distancePivotToTurnLine() const;
    void setDistancePivotToTurnLine(double value);
    QBindable<double> bindableDistancePivotToTurnLine();

    bool isYouTurnRight() const;
    void setIsYouTurnRight(bool value);
    QBindable<bool> bindableIsYouTurnRight();

    bool isYouTurnTriggered() const;
    void setIsYouTurnTriggered(bool value);
    QBindable<bool> bindableIsYouTurnTriggered();

    int current_trackNum() const;
    void setCurrent_trackNum(int value);
    QBindable<int> bindableCurrent_trackNum();

    int track_idx() const;
    void setTrack_idx(int value);
    QBindable<int> bindableTrack_idx();

    double lblmodeActualXTE() const;
    void setLblmodeActualXTE(double value);
    QBindable<double> bindableLblmodeActualXTE();

    double lblmodeActualHeadingError() const;
    void setLblmodeActualHeadingError(double value);
    QBindable<double> bindableLblmodeActualHeadingError();

    // Tool Position
    double toolLatitude() const;
    void setToolLatitude(double value);
    QBindable<double> bindableToolLatitude();

    double toolLongitude() const;
    void setToolLongitude(double value);
    QBindable<double> bindableToolLongitude();

    // Wizard/Calibration
    int sampleCount() const;
    void setSampleCount(int value);
    QBindable<int> bindableSampleCount();

    double confidenceLevel() const;
    void setConfidenceLevel(double value);
    QBindable<double> bindableConfidenceLevel();

    bool hasValidRecommendation() const;
    void setHasValidRecommendation(bool value);
    QBindable<bool> bindableHasValidRecommendation();

    bool startSA() const;
    void setStartSA(bool value);
    QBindable<bool> bindableStartSA();

    // Visual Geometry
    QVariant vehicle_xy() const;
    void setVehicle_xy(const QVariant& value);
    QBindable<QVariant> bindableVehicle_xy();

    QVariant vehicle_bounding_box() const;
    void setVehicle_bounding_box(const QVariant& value);
    QBindable<QVariant> bindableVehicle_bounding_box();

    // Misc Status
    bool steerSwitchHigh() const;
    void setSteerSwitchHigh(bool value);
    QBindable<bool> bindableSteerSwitchHigh();

    bool imuCorrected() const;
    void setImuCorrected(bool value);
    QBindable<bool> bindableImuCorrected();

    QString lblCalcSteerAngleInner() const;
    void setLblCalcSteerAngleInner(const QString &value);
    QBindable<QString> bindableLblCalcSteerAngleInner();

    QString lblDiameter() const;
    void setLblDiameter(const QString &value);
    QBindable<QString> bindableLblDiameter();

    int droppedSentences() const;
    void setDroppedSentences(int value);
    QBindable<int> bindableDroppedSentences();

    // Field Data
    double areaOuterBoundary() const;
    void setAreaOuterBoundary(double value);
    QBindable<double> bindableAreaOuterBoundary();

    double areaBoundaryOuterLessInner() const;
    void setAreaBoundaryOuterLessInner(double value);
    QBindable<double> bindableAreaBoundaryOuterLessInner();

    double workedAreaTotal() const;
    void setWorkedAreaTotal(double value);
    QBindable<double> bindableWorkedAreaTotal();

    double workedAreaTotalUser() const;
    void setWorkedAreaTotalUser(double value);
    QBindable<double> bindableWorkedAreaTotalUser();

    double distanceUser() const;
    void setDistanceUser(double value);
    QBindable<double> bindableDistanceUser();

    double actualAreaCovered() const;
    void setActualAreaCovered(double value);
    QBindable<double> bindableActualAreaCovered();

    double userSquareMetersAlarm() const;
    void setUserSquareMetersAlarm(double value);
    QBindable<double> bindableUserSquareMetersAlarm();

    bool isContourBtnOn() const;
    void setIsContourBtnOn(bool value);
    QBindable<bool> bindableIsContourBtnOn();

    bool isYouTurnBtnOn() const;
    void setIsYouTurnBtnOn(bool value);
    QBindable<bool> bindableIsYouTurnBtnOn();

    int sensorData() const;
    void setSensorData(int value);
    QBindable<int> bindableSensorData();

    bool btnIsContourLocked() const;
    void setBtnIsContourLocked(bool value);
    QBindable<bool> bindableBtnIsContourLocked();

    // GPS/NMEA Coordinates
    double latStart() const;
    void setLatStart(double value);
    QBindable<double> bindableLatStart();

    double lonStart() const;
    void setLonStart(double value);
    QBindable<double> bindableLonStart();

    // Geodetic Conversion - Phase 6.0.20 Task 24 Step 3.5
    // Simple getter - no Q_PROPERTY (C++ only, not exposed to QML)
    double mPerDegreeLat() const { return m_mPerDegreeLat; }

    // Geodetic Conversion Functions - Phase 6.0.20 Task 24 Step 3.5
    // Exposed to QML for coordinate transformations
    Q_INVOKABLE QVariantList convertLocalToWGS84(double northing, double easting);
    Q_INVOKABLE QVariantList convertWGS84ToLocal(double latitude, double longitude);

    // NMEA Processing
    uint sentenceCounter() const;
    void setSentenceCounter(uint value);
    QBindable<uint> bindableSentenceCounter();

    // GPS/IMU Heading - Phase 6.0.20 Task 24 Step 2
    double gpsHeading() const;
    void setGpsHeading(double value);
    QBindable<double> bindableGpsHeading();

    bool isReverseWithIMU() const;
    void setIsReverseWithIMU(bool value);
    QBindable<bool> bindableIsReverseWithIMU();

    // Module Connection - Phase 6.0.20 Task 24 Step 3.2
    int steerModuleConnectedCounter() const;
    void setSteerModuleConnectedCounter(int value);
    QBindable<int> bindableSteerModuleConnectedCounter();

    // Button States
    int autoBtnState() const;
    void setAutoBtnState(int value);
    QBindable<int> bindableAutoBtnState();

    int manualBtnState() const;
    void setManualBtnState(int value);
    QBindable<int> bindableManualBtnState();

    bool autoTrackBtnState() const;
    void setAutoTrackBtnState(bool value);
    QBindable<bool> bindableAutoTrackBtnState();

    bool autoYouturnBtnState() const;
    void setAutoYouturnBtnState(bool value);
    QBindable<bool> bindableAutoYouturnBtnState();

    // Job Control
    bool isPatchesChangingColor() const;
    void setIsPatchesChangingColor(bool value);
    QBindable<bool> bindableIsPatchesChangingColor();

    // Boundary Interface
    bool isOutOfBounds() const;
    void setIsOutOfBounds(bool value);
    QBindable<bool> bindableIsOutOfBounds();

    bool isHeadlandOn() const;
    void setIsHeadlandOn(bool value);
    QBindable<bool> bindableIsHeadlandOn();

    double createBndOffset() const;
    void setCreateBndOffset(double value);
    QBindable<double> bindableCreateBndOffset();

    bool isDrawRightSide() const;
    void setIsDrawRightSide(bool value);
    QBindable<bool> bindableIsDrawRightSide();

    // RecordedPath Interface
    bool isDrivingRecordedPath() const;
    void setIsDrivingRecordedPath(bool value);
    QBindable<bool> bindableIsDrivingRecordedPath();

    QString recordedPathName() const;
    void setRecordedPathName(const QString& value);
    QBindable<QString> bindableRecordedPathName();

    // Boundary State
    bool boundaryIsRecording() const;
    void setBoundaryIsRecording(bool value);
    QBindable<bool> bindableBoundaryIsRecording();

    double boundaryArea() const;
    void setBoundaryArea(double value);
    QBindable<double> bindableBoundaryArea();

    int boundaryPointCount() const;
    void setBoundaryPointCount(int value);
    QBindable<int> bindableBoundaryPointCount();

     /***********************************************
     * Qt-specific things we need to keep track of *
     ***********************************************/
    QLocale locale;
    QObject *mainWindow;
    QSignalMapper *sectionButtonsSignalMapper;
    QTimer *tmrWatchdog;
    QTimer timerSim;
    QTimer timerGPS;  // Phase 6.0.24: Fixed 40 Hz timer for real GPS mode (like timerSim for simulation)
    QTimer *timer_tick;

    //other
    QObject *btnFlagObject;

    /***************************
     * Qt and QML GUI elements *
     ***************************/
    //QQuickView *qmlview;
    QWidget *qmlcontainer;
    QQuickItem *openGLControl;

    //flag context menu and buttons
    QObject *contextFlag;
    QObject *boundaryInterface;
    QObject *fieldInterface;
    QObject *recordedPathInterface;
    QObject *btnDeleteFlag;
    QObject *btnDeleteAllFlags;

    //section buttons
    QObject *sectionButton[MAXSECTIONS-1]; //zero based array

    QObject *txtDistanceOffABLine;

    //offscreen GL objects:
    QSurfaceFormat backSurfaceFormat;
    QOpenGLContext backOpenGLContext;
    QOffscreenSurface backSurface;
    std::unique_ptr<QOpenGLFramebufferObject> backFBO; // C++17 RAII - automatic cleanup

    QSurfaceFormat zoomSurfaceFormat;
    QOpenGLContext zoomOpenGLContext;
    QOffscreenSurface zoomSurface;
    std::unique_ptr<QOpenGLFramebufferObject> zoomFBO; // C++17 RAII - automatic cleanup

    QSurfaceFormat mainSurfaceFormat;
    QOpenGLContext mainOpenGLContext;
    QOffscreenSurface mainSurface;

    std::unique_ptr<QOpenGLFramebufferObject> mainFBO[2]; // C++17 RAII - automatic cleanup
    int active_fbo=-1;

    /*******************
     * from FormGPS.cs *
     *******************/
    //The base directory where AgOpenGPS will be stored and fields and vehicles branch from
    QString baseDirectory;

    //current directory of vehicle
    QString vehiclesDirectory, vehicleFileName = "";

    //current directory of tools
    QString toolsDirectory, toolFileName = "";

    //current directory of Environments
    QString envDirectory, envFileName = "";

    //current fields and field directory
    QString fieldsDirectory, currentFieldDirectory, displayFieldName;

    // android directory
    QString androidDirectory = "/storage/emulated/0/Documents/";


    bool leftMouseDownOnOpenGL; //mousedown event in opengl window
    int flagNumberPicked = 0;

    //bool for whether or not a job is active
    bool /*setIsJobStarted(false),*/ isAreaOnRight = true /*, setIsBtnAutoSteerOn(false)*/;

    //this bool actually lives in the QML aog object.
    // ⚡ PHASE 6.3.0: Migrated to Q_PROPERTY system above
    // InterfaceProperty<AOGInterface,bool> isJobStarted = InterfaceProperty<AOGInterface,bool>("isJobStarted");
    // InterfaceProperty<AOGInterface,bool> isBtnAutoSteerOn = InterfaceProperty<AOGInterface,bool>("isBtnAutoSteerOn");

    //if we are saving a file
    bool isSavingFile = false, isLogElevation = false;

    //the currentversion of software
    QString currentVersionStr, inoVersionStr;

    int inoVersionInt = 0;  // Phase 6.0.24 Problem 18: Initialize to prevent garbage

    //create instance of a stopwatch for timing of frames and NMEA hz determination
    QElapsedTimer swFrame;

    double secondsSinceStart = 0.0;  // Phase 6.0.24 Problem 18: Initialize to prevent garbage

    //Time to do fix position update and draw routine
    double gpsHz = 10;

    bool isStanleyUsed = false;

    // Phase 6.0.24 Problem 18: Initialize progress bar values
    int pbarSteer = 0;
    int pbarRelay = 0;
    int pbarUDP = 0;

    double nudNumber = 0;

    //used by filePicker Form to return picked file and directory
    //QString filePickerFileAndDirectory;

    //the position of the GPS Data window within the FormGPS window
    //int GPSDataWindowLeft = 76, GPSDataWindowTopOffset = 220;

    //the autoManual drive button. Assume in Auto
    bool isInAutoDrive = true;

    //isGPSData form up
    bool isGPSSentencesOn = false, isKeepOffsetsOn = false;


private:
    // AgIO Service (main thread for zero-latency OpenGL access)
    AgIOService* m_agioService;

    // ⚡ QML Interface Initialization - Delayed to avoid timing issues
    void initializeQMLInterfaces();
    // ⚡ OpenGL Callbacks Setup - With InterfaceProperty validation
    void initializeOpenGLCallbacks();

    // ⚡ Safe QML Object Access - With NULL protection and retries
    QObject* safeQmlItem(const QString& objectName, int maxRetries = 3);

    // Geodetic Conversion - Phase 6.0.20 Task 24 Step 3.5
    void updateMPerDegreeLat();

    // ⚡ PHASE 6.0.3.2: Constructor completion protection
    // Qt 6.8: Simplified - no complex state tracking needed
    
    //For field saving in background
    int fileSaveCounter = 1;
    int minuteCounter = 1;

    int tenMinuteCounter = 1;

    //used to update the screen status bar etc
    int displayUpdateHalfSecondCounter = 0, displayUpdateOneSecondCounter = 0, displayUpdateOneFifthCounter = 0, displayUpdateThreeSecondCounter = 0;
    int tenSecondCounter = 0, tenSeconds = 0;
    int threeSecondCounter = 0, threeSeconds = 0;
    int oneSecondCounter = 0, oneSecond = 0;
    int oneHalfSecondCounter = 0, oneHalfSecond = 0;
    int oneFifthSecondCounter = 0, oneFifthSecond = 0;

    //moved to CYouTurn
    //int makeUTurnCounter = 0;


     /*******************
     * GUI.Designer.cs *
     *******************/
public:
    //ABLines directory
    QString ablinesdirectory;

    //colors for sections and field background
    int flagColor = 0;

    //how many cm off line per big pixel
    int lightbarCmPerPixel = 2;

    //polygon mode for section drawing
    bool isDrawPolygons = false;

    QColor frameDayColor;
    QColor frameNightColor;
    QColor sectionColorDay;
    QColor fieldColorDay;
    QColor fieldColorNight;

    QColor textColorDay;
    QColor textColorNight;

    QColor vehicleColor;
    double vehicleOpacity;
    uchar vehicleOpacityByte;
    bool isVehicleImage;

    //Is it in 2D or 3D, metric or imperial, display lightbar, display grid etc
    bool isMetric = true, isLightbarOn = true, isGridOn, isFullScreen;
    bool isUTurnAlwaysOn, isCompassOn, isSpeedoOn, isSideGuideLines = true;
    bool isPureDisplayOn = true, isSkyOn = true, isRollMeterOn = false, isTextureOn = true;
    bool isDay = true, isDayTime = true, isBrightnessOn = true;
    bool isKeyboardOn = true, isAutoStartAgIO = true, isSvennArrowOn = true;
    bool isConnectedBlockage = false; //Dim
    bool isUTurnOn = true, isLateralOn = true;

    //sunrise, sunset

    bool isFlashOnOff = false;
    //makes nav panel disappear after 6 seconds
    int navPanelCounter = 0;

    // ✅ PHASE 6.3.0: sentenceCounter converted to Q_PROPERTY in AOGInterface.qml:154

    //master Manual and Auto, 3 states possible
    //btnStates manualBtnState = btnStates::Off;
    //btnStates autoBtnState = btnStates::Off;
    // ⚡ PHASE 6.3.0: manualBtnState and autoBtnState converted to Q_PROPERTY
    // Access via: qmlItem(mainWindow, "aog")->property("manualBtnState").toInt()
    // Access via: qmlItem(mainWindow, "aog")->property("autoBtnState").toInt()
    //InterfaceProperty<AOGInterface,bool> ct.isLocked = InterfaceProperty<AOGInterface,bool>("btnIsContourLocked");

private:
public:
    //for animated submenu
    //bool isMenuHid = true;

    // Storage For Our Tractor, implement, background etc Textures
    //Texture particleTexture;

    QElapsedTimer stopwatch; //general stopwatch for debugging purposes.
    //readonly Stopwatch swFrame = new Stopwatch();



    //Time to do fix position update and draw routine
    double HzTime = 5;

    QVector<CPatches> triStrip = QVector<CPatches>( { CPatches() } );


    //used to update the screen status bar etc
    int statusUpdateCounter = 1;

    //create the scene camera
    CCamera camera;

    //create world grid
    //QScopedPointer <CWorldGrid> worldGrid;
    CWorldGrid worldGrid;

    //Parsing object of NMEA sentences
    //QScopedPointer<CNMEA> pn;
    CNMEA pn;

    //ABLine Instance
    //QScopedPointer<CABLine> ABLine;

    // Track management - restored from original architecture (was CTrack trk)
    CTrack track;

    CGuidance gyd;

    CTram tram;

    //Contour mode Instance
    //QScopedPointer<CContour> ct;
    CContour ct;
    CYouTurn yt;

    CVehicle* vehicle;  // Pointeur vers singleton
    CTool tool;

    //module communication object
    CModuleComm mc;

    //boundary instance
    CBoundary bnd;

    CSim sim;
    CAHRS ahrs;
    CRecordedPath recPath;
    CFieldData fd;

    CHeadLine hdl;

    FormHeadland headland_form;
    FormHeadache headache_form;

    /*
     * PGNs *
     */
    CPGN_FE p_254;
    CPGN_FC p_252;
    CPGN_FB p_251;
    CPGN_EF p_239;
    CPGN_EE p_238;
    CPGN_EC p_236;
    CPGN_EB p_235;
    CPGN_E5 p_229;

    /* GUI synchronization lock */
    QReadWriteLock lock;
    bool newframe = false;

    bool bootstrap_field = false;
   /************************
     * Controls.Designer.cs *
     ************************/
public:
    bool isTT;
    bool isABCyled = false;

    // ⚡ PHASE 6.3.0: isPatchesChangingColor converted to Q_PROPERTY
    // Access via: qmlItem(mainWindow, "aog")->property("isPatchesChangingColor").toBool()

    void GetHeadland();
    void CloseTopMosts();
    void getAB();
    void FixTramModeButton();

    //other things will be in slots

    /*************************
     *  Position.designer.cs *
     *************************/
public:
    //very first fix to setup grid etc
    bool isFirstFixPositionSet = false, isGPSPositionInitialized = false, isFirstHeadingSet = false;
    bool m_forceGPSReinitialization = false;  // PHASE 6.0.41: Force latStart/lonStart update on mode switch even if field open
    bool /*isReverse = false (CVehicle),*/ isSteerInReverse = true, isSuperSlow = false, isAutoSnaptoPivot = false;
    double startGPSHeading = 0;

    //string to record fixes for elevation maps
    QByteArray sbGrid;

    // autosteer variables for sending serial moved to CVehicle
    //short guidanceLineDistanceOff, guidanceLineSteerAngle; --> CVehicle
    double avGuidanceSteerAngle = 0.0;  // Phase 6.0.24 Problem 18

    short errorAngVel = 0;  // Phase 6.0.24 Problem 18
    double setAngVel = 0.0;  // Phase 6.0.24 Problem 18
    double actAngVel = 0.0;  // Phase 6.0.24 Problem 18
    bool isConstantContourOn;

    //guidance line look ahead
    double guidanceLookAheadTime = 2;

    //for heading or Atan2 as camera
    QString headingFromSource, headingFromSourceBak;

    /* moved to CVehicle:
    Vec3 pivotAxlePos;
    Vec3 steerAxlePos;
    Vec3 toolPos;
    Vec3 tankPos;
    Vec3 hitchPos;
    */

    //history
    Vec2 prevFix;
    Vec2 prevDistFix;
    Vec2 lastReverseFix;

    //headings
    double smoothCamHeading = 0, prevGPSHeading = 0.0;
    // gpsHeading moved to Q_OBJECT_BINDABLE_PROPERTY m_gpsHeading (line 1847)

    //storage for the cos and sin of heading
    //moved to vehicle
    //double cosSectionHeading = 1.0, sinSectionHeading = 0.0;

    //how far travelled since last section was added, section points
    double sectionTriggerDistance = 0, contourTriggerDistance = 0, sectionTriggerStepDistance = 0, gridTriggerDistance;
    Vec2 prevSectionPos;
    Vec2 prevContourPos;
    Vec2 prevGridPos;
    int patchCounter = 0;

    Vec2 prevBoundaryPos;

    //Everything is so wonky at the start
    int startCounter = 0;

    //individual points for the flags in a list
    QVector<CFlag> flagPts;
    bool flagsBufferCurrent = false;

    //tally counters for display
    //public double totalSquareMetersWorked = 0, totalUserSquareMeters = 0, userSquareMetersAlarm = 0;


    double /*avgSpeed --> CVehicle,*/ previousSpeed = 0.0;  // Phase 6.0.24 Problem 18
    double crossTrackError = 0.0;  // Phase 6.0.24 Problem 18: for average cross track error

    //youturn
    double _distancePivotToTurnLine = -2222;  // Renamed to avoid Q_PROPERTY conflict
    double distanceToolToTurnLine = -2222;

    //the value to fill in you turn progress bar
    int youTurnProgressBar = 0;

    //IMU
    double rollCorrectionDistance = 0;
    // Phase 6.0.24 Problem 18: Initialize IMU variables to prevent garbage values causing crash
    // If uninitialized, _imuCorrected garbage gets copied to CVehicle::fixHeading → modelview.rotate(garbage) → nan → crash
    double imuGPS_Offset = 0.0;
    double _imuCorrected = 0.0;  // Renamed to avoid Q_PROPERTY conflict

    //step position - slow speed spinner killer
    int currentStepFix = 0;
    int totalFixSteps = 10;
    VecFix2Fix stepFixPts[10];
    double distanceCurrentStepFix = 0, distanceCurrentStepFixDisplay = 0, minHeadingStepDist, startSpeed = 0.5;
    double fixToFixHeadingDistance = 0, gpsMinimumStepDistance;  // PHASE 6.0.35: Loaded from SettingsManager in loadSettings()

    // isReverseWithIMU moved to Q_OBJECT_BINDABLE_PROPERTY m_isReverseWithIMU (line 1848)

    double nowHz = 0, filteredDelta = 0, delta = 0;

    bool isRTK, isRTK_KillAutosteer;

    double headlandDistanceDelta = 0, boundaryDistanceDelta = 0;

    Vec2 lastGPS;

    double uncorrectedEastingGraph = 0;
    double correctionDistanceGraph = 0;

    double frameTimeRough = 3;
    double timeSliceOfLastFix = 0;

    bool isMaxAngularVelocity = false;

    int minSteerSpeedTimer = 0;


    void UpdateFixPosition(); //process a new position

    void TheRest();
    void CalculatePositionHeading(); // compute all headings and fixes
    void AddBoundaryPoint();
    void AddContourPoints();
    void AddSectionOrPathPoints();
    void CalculateSectionLookAhead(double northing, double easting, double cosHeading, double sinHeading);
    void InitializeFirstFewGPSPositions();
    void ResetGPSState(bool toSimMode);  // PHASE 6.0.40: Reset GPS state on sim/real switch

    /************************
     * SaveOpen.Designer.cs *
     ************************/

    //moved to CTool
    //list of the list of patch data individual triangles for field sections
    //QVector<QSharedPointer<QVector<QVector3D>>> patchSaveList;

    //moved to CContour.
    //list of the list of patch data individual triangles for contour tracking
    QVector<QSharedPointer<QVector<Vec3>>> contourSaveList;

    void FileSaveHeadLines();
    void FileLoadHeadLines();
    //moved up to a SLOT: void FileSaveTracks();
    void FileLoadTracks();
    void FileSaveCurveLines();
    void FileLoadCurveLines();
    void FileSaveABLines();
    void FileLoadABLines();
    bool FileOpenField(QString fieldDir, int flags = -1);
    QMap<QString, QVariant> FileFieldInfo(QString fieldDir);
    void FileCreateField();
    void FileCreateElevation();
    void FileSaveSections();
    void FileCreateSections();
    void FileCreateFlags();
    void FileCreateContour();
    void FileSaveContour();
    void FileCreateBoundary();
    void FileSaveBoundary();
    void FileSaveTram();
    void FileSaveBackPic();
    void FileCreateRecPath();
    void FileSaveHeadland();
    void FileSaveRecPath();
    void FileLoadRecPath();
    void FileSaveFlags();
    void FileSaveNMEA();
    void FileSaveElevation();
    void FileSaveSingleFlagKML2(int flagNumber);
    void FileSaveSingleFlagKML(int flagNumber);
    void FileMakeKMLFromCurrentPosition(double lat, double lon);
    void ExportFieldAs_KML();
    void FileUpdateAllFieldsKML();
    QString GetBoundaryPointsLatLon(int bndNum);
    void ExportFieldAs_ISOXMLv3();
    void ExportFieldAs_ISOXMLv4();


    /************************
     * formgps_sections.cpp *
     ************************/
    //void SectionSetPosition();
    //void SectionCalcWidths();
    //void SectionCalcMulti();
    void BuildMachineByte();
    void DoRemoteSwitches();
    //void doBlockageMonitoring();


    /************************
     * formgps_settimgs.cpp *
     ************************/

    void loadSettings();

    /**********************
     * OpenGL.Designer.cs *
     **********************/
    //extracted Near, Far, Right, Left clipping planes of frustum
    double frustum[24];

    double fovy = 0.7;
    double camDistanceFactor = -2;
    int mouseX = 0, mouseY = 0;
    double mouseEasting = 0, mouseNorthing = 0;
    int lastWidth=-1, lastHeight=-1;
    // Phase 6.0.24 Problem 18: Initialize field boundary variables
    double maxFieldX = 0.0;
    double maxFieldY = 0.0;
    double minFieldX = 0.0;
    double minFieldY = 0.0;
    double fieldCenterX = 0.0;
    double fieldCenterY = 0.0;
    double maxFieldDistance = 0.0;
    double offX = 0.0, offY = 0.0;

    //data buffer for pixels read from off screen buffer
    //uchar grnPixels[80001];
    LookAheadPixels grnPixels[150001];
    LookAheadPixels *overPixels = new LookAheadPixels[160000]; //400x400
    QImage grnPix; //for debugging purposes to show in a window
    QImage overPix; //for debugging purposes to show in a window

    /*
    QOpenGLShaderProgram *simpleColorShader = 0;
    QOpenGLShaderProgram *texShader = 0;
    QOpenGLShaderProgram *interpColorShader = 0;
    */
    QOpenGLBuffer skyBuffer;
    QOpenGLBuffer flagsBuffer;

    QVector<QVector<PatchInBuffer>> patchesInBuffer;
    QVector<PatchBuffer> patchBuffer;
    bool patchesBufferDirty = true;

    /***********************
     * formgps_udpcomm.cpp *
     ***********************/
private:
    // UDP FormGPS REMOVED - Phase 4.6: Workers → AgIOService ONLY source
    // QUdpSocket *udpSocket = NULL;  // ❌ REMOVED - AgIOService Workers only

public:
    // ===== Q_INVOKABLE MODERN ACTIONS - Qt 6.8 Direct QML Calls =====
    // Phase 6.0.20: Modernization of button actions - replace signal/slot with direct calls
    Q_INVOKABLE void resetTool();
    Q_INVOKABLE void contour();
    Q_INVOKABLE void contourLock();
    Q_INVOKABLE void contourPriority(bool isRight);
    // Batch 7 actions - lines 215-222
    Q_INVOKABLE void headland();
    Q_INVOKABLE void youSkip();
    Q_INVOKABLE void resetSim();
    Q_INVOKABLE void rotateSim();
    Q_INVOKABLE void sim_bump_speed(bool increase);
    Q_INVOKABLE void sim_zero_speed();
    Q_INVOKABLE void sim_reset();
    Q_INVOKABLE void resetDirection();
    Q_INVOKABLE void centerOgl();
    Q_INVOKABLE void deleteAppliedArea();
    // Batch 2 - 7 actions You-Turn and Navigation - lines 220-226
    Q_INVOKABLE void manualUTurn(bool isRight);
    Q_INVOKABLE void lateral(bool isRight);
    Q_INVOKABLE void autoYouTurn();
    // Batch 9 - 2 actions Snap Track - lines 237-238
    Q_INVOKABLE void snapSideways(double distance);
    Q_INVOKABLE void snapToPivot();
    // Batch 10 - 8 actions Modules & Steering - lines 253-266
    Q_INVOKABLE void modulesSend238();
    Q_INVOKABLE void modulesSend251();
    Q_INVOKABLE void modulesSend252();
    Q_INVOKABLE void blockageMonitoring();
    Q_INVOKABLE void steerAngleUp();
    Q_INVOKABLE void steerAngleDown();
    Q_INVOKABLE void freeDrive();
    Q_INVOKABLE void freeDriveZero();
    Q_INVOKABLE void startSAAction();
    // Batch 11 - 9 actions Flag Management - lines 312-320
    Q_INVOKABLE void redFlag();
    Q_INVOKABLE void greenFlag();
    Q_INVOKABLE void yellowFlag();
    Q_INVOKABLE void deleteFlag();
    Q_INVOKABLE void deleteAllFlags();
    Q_INVOKABLE void nextFlag();
    Q_INVOKABLE void prevFlag();
    Q_INVOKABLE void cancelFlag();
    Q_INVOKABLE void redFlagAt(double lat, double lon, int color);

    // Batch 12 - 6 actions Wizard & Calibration - lines 325-330
    Q_INVOKABLE void stopDataCollection();
    Q_INVOKABLE void startDataCollection();
    Q_INVOKABLE void resetData();
    Q_INVOKABLE void applyOffsetToCollectedData(double offset);
    Q_INVOKABLE void smartCalLabelClick();
    Q_INVOKABLE void smartZeroWAS();

    // Batch 13 - 7 actions Field Management - lines 1826-1832
    Q_INVOKABLE void fieldUpdateList();
    Q_INVOKABLE void fieldClose();
    Q_INVOKABLE void fieldOpen(const QString& fieldName);
    Q_INVOKABLE void fieldNew(const QString& fieldName);
    Q_INVOKABLE void fieldNewFrom(const QString& fieldName, const QString& sourceField, int fieldType);
    Q_INVOKABLE void fieldNewFromKML(const QString& fieldName, const QString& kmlPath);
    Q_INVOKABLE void fieldDelete(const QString& fieldName);

    // Batch 14 - 11 actions Boundary Management - lines 1843-1854
    Q_INVOKABLE void boundaryCalculateArea();
    Q_INVOKABLE void boundaryUpdateList();
    Q_INVOKABLE void boundaryStart();
    Q_INVOKABLE void boundaryStop();
    Q_INVOKABLE void boundaryAddPoint();
    Q_INVOKABLE void boundaryDeleteLastPoint();
    Q_INVOKABLE void boundaryPause();
    Q_INVOKABLE void boundaryRecord();
    Q_INVOKABLE void boundaryReset();
    Q_INVOKABLE void boundaryDeleteBoundary(int boundaryId);
    Q_INVOKABLE void boundarySetDriveThrough(int boundaryId, bool isDriveThrough);
    Q_INVOKABLE void boundaryDeleteAll();

    // RecordedPath Management (6 methods) - ZERO EMIT
    Q_INVOKABLE void recordedPathUpdateLines();
    Q_INVOKABLE void recordedPathOpen(const QString& pathName);
    Q_INVOKABLE void recordedPathDelete(const QString& pathName);
    Q_INVOKABLE void recordedPathStartDriving();
    Q_INVOKABLE void recordedPathStopDriving();
    Q_INVOKABLE void recordedPathClear();

    Q_INVOKABLE void swapAutoYouTurnDirection();
    Q_INVOKABLE void resetCreatedYouTurn();
    Q_INVOKABLE void autoTrack();
    Q_INVOKABLE void flag();
    // Batch 3 - 8 actions Camera Navigation - lines 201-208
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void tiltDown();
    Q_INVOKABLE void tiltUp();
    Q_INVOKABLE void view2D();
    Q_INVOKABLE void view3D();
    Q_INVOKABLE void normal2D();
    Q_INVOKABLE void normal3D();
    // Batch 4 - 2 actions Settings - lines 227-228
    Q_INVOKABLE void settingsReload();
    Q_INVOKABLE void settingsSave();

    // AB Lines and Curves management - Qt 6.8 additions
    Q_INVOKABLE void updateABLines();
    Q_INVOKABLE void updateCurves();
    Q_INVOKABLE void setCurrentABCurve(int index);

    // AB Lines Methods - Phase 6.0.20
    Q_INVOKABLE void swapABLineHeading(int index);
    Q_INVOKABLE void deleteABLine(int index);
    Q_INVOKABLE void addABLine(const QString& name);
    Q_INVOKABLE void changeABLineName(int index, const QString& newName);

    // ===== Q_INVOKABLE ALIASES FOR QML CONSISTENCY =====
    Q_INVOKABLE void settings_save() { settingsSave(); }
    Q_INVOKABLE void settings_revert() { settingsReload(); }
    Q_INVOKABLE void btnFlag() { flag(); }
    // modules_send_252 not needed - modulesSend252() already exists as Q_INVOKABLE

    // ===== Q_INVOKABLE IMU CONFIGURATION =====
    Q_INVOKABLE void changeImuHeading(double heading);
    Q_INVOKABLE void changeImuRoll(double roll);

    // ===== Q_INVOKABLE USER DATA MANAGEMENT =====
    Q_INVOKABLE void setDistanceUser(const QString& value);
    Q_INVOKABLE void setWorkedAreaTotalUser(const QString& value);
    // Phase 6.0.20: setAvgPivDistance is now Q_PROPERTY setter (line 471) - no Q_INVOKABLE needed

    // Qt BINDABLE: Automatic property synchronization - no manual sync needed

    // UDP FormGPS variables REMOVED - Phase 4.6: AgIOService Workers only
    // QElapsedTimer udpWatch;        // ❌ REMOVED
    // int udpWatchLimit = 70;        // ❌ REMOVED
    // int udpWatchCounts = 0;        // ❌ REMOVED
    // bool isUDPServerOn = false;    // ❌ REMOVED

    // UDP FormGPS methods REMOVED - Phase 4.6: AgIOService Workers only
    // void StartLoopbackServer();    // ❌ REMOVED
    // void stopUDPServer();          // ❌ REMOVED

    // void SendPgnToLoop(QByteArray byteData);  // ❌ REMOVED - AgIOService Workers only
    void DisableSim();
    void LoadKMLBoundary(const std::string& filename);
    // void ReceiveFromAgIO(); // ❌ REMOVED - AgIOService Workers only

    /******************
     * formgps_ui.cpp *
     ******************/
    //or should be under formgps_settings.cpp?

   /**********************
     * OpenGL.Designer.cs *
     **********************/
    ulong number = 0, lastNumber = 0;

    bool isHeadlandClose = false;

    // steerModuleConnectedCounter moved to Q_OBJECT_BINDABLE_PROPERTY m_steerModuleConnectedCounter (line 1860)
    double lightbarDistance=0;
    QString strHeading;
    int lenth = 4;

    void MakeFlagMark(QOpenGLFunctions *gl);
    void DrawFlags(QOpenGLFunctions *gl, QMatrix4x4 mvp);
    void DrawTramMarkers();
    void CalcFrustum(const QMatrix4x4 &mvp);
    void calculateMinMax();

    QVector3D mouseClickToField(int mouseX, int mouseY);
    QVector3D mouseClickToPan(int mouseX, int mouseY);

    void loadGLTextures();
    void Timer1_Tick();

private:
    bool toSend = false, isSA = false;
    int counter = 0, secondCntr = 0, cntr = 0;  // Phase 6.0.24 Problem 18
    Vec3 startFix;
    // Phase 6.0.24 Problem 18: Initialize steer wizard variables
    double _diameter = 0.0;  // Renamed to avoid Q_PROPERTY conflict
    double steerAngleRight = 0.0;
    double dist = 0.0;
    // DEAD CODE from C# original - lblCalcSteerAngleOuter never displayed in UI (FormSteerWiz.Designer.cs has no widget)
    // C# FormSteer.cs lines 335, 848, 854: all assignments commented out
    // Qt QtAgOpenGPS formgps_position.cpp:1253: setProperty() was dead code
    // TODO Phase 7: Remove all dead code comments
    QString lblCalcSteerAngleOuter;  // lblCalcSteerAngleInner and lblDiameter migrated to Q_PROPERTY BINDABLE

    // Language translation system
    QTranslator* m_translator;

    // PHASE 6.0.42: GPS jump detection for automatic field close and OpenGL regeneration
    double m_lastKnownLatitude = 0;
    double m_lastKnownLongitude = 0;
    const double GPS_JUMP_THRESHOLD_KM = 1.0;  // 1 km threshold for jump detection

private:
    void setupGui();
    void setupAgIOService();
    void connectToAgIOFactoryInstance(); // New: connect to factory-created instance
    void testAgIOConfiguration();
    void connectFormLoopToAgIOService();
    void cleanupAgIOService();

    // PHASE 6.0.42: GPS jump detection helper functions
    bool detectGPSJump(double newLat, double newLon);
    void handleGPSJump(double newLat, double newLon);


    /**************
     * FormGPS.cs *
     **************/
public:
    QString speedMPH();
    QString speedKPH();

    void JobNew();
    void JobClose();

    /******************************
     * formgps_classcallbacks.cpp *
     ******************************/
    void connect_classes();


    /****************
     * form_sim.cpp *
     ****************/
    void simConnectSlots();

    /**************************
     * UI/Qt object callbacks *
     **************************/

public slots:
    // Phase 6.0.21: Receive parsed data from AgIOService
    void onParsedDataReady(const PGNParser::ParsedData& data);

    // Phase 6.0.25: Separated data handlers for optimal performance
    void onNmeaDataReady(const PGNParser::ParsedData& data);    // GPS position updates
    void onImuDataReady(const PGNParser::ParsedData& data);     // External IMU updates
    void onSteerDataReady(const PGNParser::ParsedData& data);   // AutoSteer feedback

    /*******************
     * from FormGPS.cs *
     *******************/
    void tmrWatchdog_timeout();
    void processSectionLookahead(); //called when section lookahead GL stuff is rendered
    void processOverlapCount(); //called to calculate overlap stats

    void TimedMessageBox(int timeout, QString s1, QString s2);

    void on_qml_created(QObject *object, const QUrl &url);

    // Qt 6.8: Simplified - removed complex property binding slots

    //settings dialog callbacks
    void on_settings_reload();
    void on_settings_save();
    void on_language_changed(); // Dynamic language switching

    //vehicle callbacks
    void vehicle_saveas(QString vehicle_name);
    //void vehicle_load(int index);
    void vehicle_load(QString vehicle_name);
    void vehicle_delete(QString vehicle_name);
    void vehicle_update_list();


    //field callbacks
    void field_update_list();
    void field_close();
    void field_open(QString field_name);
    void field_new(QString field_name);
    void field_new_from(QString existing, QString field_name, int flags);
    void field_new_from_KML(QString field_name, QString file_name);
    void field_delete(QString field_name);
    void field_saveas(QString field_name);
    void field_load_json(QString field_name);

    //modules ui callback
    void modules_send_238();
    void modules_send_251();
    void modules_send_252();
    // Note: modulesSend238/251/252 are Q_INVOKABLE versions for QML

    void doBlockageMonitoring();

    //boundary UI for recording new boundary
    void boundary_calculate_area();
    void boundary_update_list();
    void boundary_start();
    void boundary_stop();
    void boundary_add_point();
    void boundary_delete_last_point();
    void boundary_pause();
    void boundary_record();
    void boundary_restart();
    void boundary_delete(int which_boundary);
    void boundary_set_drivethru(int which_boundary, bool drive_through);
    void boundary_delete_all();

    void headland_save();
    void headlines_save();
    void headlines_load();

    //headland creation

    void onBtnResetDirection_clicked();
    //left column
    void onBtnAgIO_clicked();
    //right column
    void onBtnContour_clicked();
    void onBtnAutoYouTurn_clicked();
    void onBtnSwapAutoYouTurnDirection_clicked();
    void onBtnContourPriority_clicked(bool isRight);
    void onBtnContourLock_clicked();
    void onBtnResetCreatedYouTurn_clicked();
    void onBtnAutoTrack_clicked();
    //bottom row
    void onBtnResetTool_clicked();
    void onBtnHeadland_clicked();
    void onBtnHydLift_clicked();
    void onBtnFlag_clicked();
    void onBtnTramlines_clicked();
    void onBtnSnapSideways_clicked(double distance);
    void onBtnSnapToPivot_clicked();
    //don't need ablineedit
    void onBtnYouSkip_clicked();


    //displaybuttons.qml
    void onBtnTiltDown_clicked();
    void onBtnTiltUp_clicked();
    void onBtn2D_clicked();
    void onBtn3D_clicked();
    void onBtnN2D_clicked();
    void onBtnN3D_clicked();

    void onBtnZoomIn_clicked();
    void onBtnZoomOut_clicked();

    void onBtnRedFlag_clicked();
    void onBtnGreenFlag_clicked();
    void onBtnYellowFlag_clicked();
    void onBtnDeleteFlag_clicked();
    void onBtnDeleteAllFlags_clicked();
    void onBtnNextFlag_clicked();
    void onBtnPrevFlag_clicked();
    void onBtnCancelFlag_clicked();
    void onBtnRed_clicked(double lat, double lon, int color);

    void SwapDirection();
    void turnOffBoundAlarm();

    void onBtnManUTurn_clicked(bool right); //TODO add the skip number as a parameter
    void onBtnLateral_clicked(bool right); //TODO add the skip number as a parameter
    void onBtnResetSim_clicked();
    void onBtnRotateSim_clicked();

    void onBtnCenterOgl_clicked();

    void onDeleteAppliedArea_clicked();

    void btnSteerAngleUp_clicked(); // steersetup
    void btnSteerAngleDown_clicked();
    void btnFreeDrive_clicked();
    void btnFreeDriveZero_clicked();
    void btnStartSA_clicked();



    /***************************
     * from OpenGL.Designer.cs *
     ***************************/
    void render_main_fbo();
    void oglMain_Paint();
    void openGLControl_Initialized();
    void openGLControl_Shutdown();
    //void openGLControl_Resize();
    void onGLControl_clicked(const QVariant &event);
    void onGLControl_dragged(int startX, int startY, int mouseX, int mouseY);

    void oglBack_Paint();
    void openGLControlBack_Initialized();

    void oglZoom_Paint();

    /***
     * UDPCOMM.Designer.cs
     * formgps_udpcomm.cpp
     ***/
    // void ReceiveFromAgIO(); // ❌ REMOVED - AgIOService Workers handle all UDP communication

    /*******************
     * simulator       *
     * formgps_sim.cpp *
     *******************/
    void onSimNewPosition(double vtgSpeed,
                     double headingTrue,
                     double latitude,
                     double longitude, double hdop,
                     double altitude,
                     double satellitesTracked);

    void onSimNewSteerAngle(double steerAngleAve);

    void onSimTimerTimeout();

    // Phase 6.0.24: GPS timer callback for real GPS mode (40 Hz fixed rate)
    void onGPSTimerTimeout();

    /*
     * misc
     */
    void FileSaveEverythingBeforeClosingField(bool saveVehicle = true);
    void FileSaveTracks();

    /* formgps_classcallbacks.cpp */
    void onStopAutoSteer(); //cancel autosteer and ensure button state
    void onSectionMasterAutoOff();
    void onSectionMasterManualOff();
    void onStoppedDriving();

signals:
    void do_processSectionLookahead();
    void do_processOverlapCount();

    // ===== Q_PROPERTY SIGNALS - Qt 6.8 Rectangle Pattern NOTIFY signals =====
    // CRITICAL: All properties need NOTIFY signals for QML bindings to work properly

    // Application State signals
    void isJobStartedChanged();
    void isBtnAutoSteerOnChanged();
    void applicationClosingChanged();  // CRITICAL: for save_everything fix

    // Position GPS signals
    void latitudeChanged();
    void longitudeChanged();
    void altitudeChanged();
    void eastingChanged();
    void northingChanged();
    void headingChanged();
    void sectionButtonStateChanged();

    // Vehicle State signals
    void speedKphChanged();
    void fusedHeadingChanged();
    void toolEastingChanged();
    void toolNorthingChanged();
    void toolHeadingChanged();
    void offlineDistanceChanged();

    // Steering Control signals
    void steerAngleActualChanged();
    void steerAngleSetChanged();
    void lblPWMDisplayChanged();
    void calcSteerAngleInnerChanged();
    void calcSteerAngleOuterChanged();
    void diameterChanged();

    // IMU Data signals
    void imuRollChanged();
    void imuPitchChanged();
    void imuHeadingChanged();
    void imuRollDegreesChanged();
    void imuAngVelChanged();
    void yawRateChanged();

    // GPS Status signals
    void hdopChanged();
    void ageChanged();
    void fixQualityChanged();
    void satellitesTrackedChanged();
    void hzChanged();
    void rawHzChanged();

    // Blockage Sensors signals
    void blockage_avgChanged();
    void blockage_min1Changed();
    void blockage_min2Changed();
    void blockage_maxChanged();
    void blockage_min1_iChanged();
    void blockage_min2_iChanged();
    void blockage_max_iChanged();
    void blockage_blockedChanged();
    void avgPivDistanceChanged();
    void frameTimeChanged();

    // Navigation signals
    void distancePivotToTurnLineChanged();
    void isYouTurnRightChanged();
    void isYouTurnTriggeredChanged();
    void current_trackNumChanged();
    void track_idxChanged();
    void lblmodeActualXTEChanged();
    void lblmodeActualHeadingErrorChanged();

    // All other property signals (continuing the pattern...)
    void toolLatitudeChanged();
    void toolLongitudeChanged();
    void sampleCountChanged();
    void confidenceLevelChanged();
    void hasValidRecommendationChanged();
    void startSAChanged();
    void vehicle_xyChanged();
    void vehicle_bounding_boxChanged();
    void steerSwitchHighChanged();
    void imuCorrectedChanged();
    void lblCalcSteerAngleInnerChanged();
    void lblDiameterChanged();
    void droppedSentencesChanged();
    void areaOuterBoundaryChanged();
    void areaBoundaryOuterLessInnerChanged();
    void workedAreaTotalChanged();
    void workedAreaTotalUserChanged();
    void distanceUserChanged();
    void actualAreaCoveredChanged();
    void userSquareMetersAlarmChanged();
    void isContourBtnOnChanged();
    void isYouTurnBtnOnChanged();
    void sensorDataChanged();
    void btnIsContourLockedChanged();
    void latStartChanged();
    void lonStartChanged();
    void mPerDegreeLatChanged();
    void sentenceCounterChanged();
    void gpsHeadingChanged();
    void isReverseWithIMUChanged();
    void steerModuleConnectedCounterChanged();
    void autoBtnStateChanged();
    void manualBtnStateChanged();
    void autoTrackBtnStateChanged();
    void autoYouturnBtnStateChanged();
    void isPatchesChangingColorChanged();
    void isOutOfBoundsChanged();
    void isHeadlandOnChanged();
    void createBndOffsetChanged();
    void isDrawRightSideChanged();
    void isDrivingRecordedPathChanged();
    void recordedPathNameChanged();
    void boundaryIsRecordingChanged();
    void boundaryAreaChanged();
    void boundaryPointCountChanged();

    // ===== SIGNALS CLEANED - Qt 6.8 Q_INVOKABLE Migration =====
    // All button action signals migrated to Q_INVOKABLE direct calls
    // See Q_INVOKABLE section above for modern implementations
signals:
    // Only non-button signals remain - no duplicates with Q_INVOKABLE

public:

    /*******************
     * Tranlation function*
     * formgps_ui.cpp *
     *******************/

private:
    // OLD translator removed - now using m_translator

    // PHASE 6.0.33: Two-buffer pattern for position stability
    // Separates RAW GPS position (immutable) from CORRECTED position (computed)
    // Prevents cascade corrections when timer calls UpdateFixPosition() between GPS packets
    Vec2 m_rawGpsPosition;          // RAW GPS position (8 Hz updates, never corrected)
    QMutex m_rawGpsPositionMutex;   // Thread-safe access (onNmeaDataReady writes, UpdateFixPosition reads)

    // ===== Q_PROPERTY MEMBER VARIABLES =====
    // 69 members for optimized properties

    // Application State (3) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isJobStarted, &FormGPS::isJobStartedChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isBtnAutoSteerOn, &FormGPS::isBtnAutoSteerOnChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_applicationClosing, &FormGPS::applicationClosingChanged)

    // ⚡ PHASE 6.0.3.2: QML Interface Ready State

    // Position GPS (6) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_latitude, &FormGPS::latitudeChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_longitude, &FormGPS::longitudeChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_altitude, &FormGPS::altitudeChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_easting, &FormGPS::eastingChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_northing, &FormGPS::northingChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_heading, &FormGPS::headingChanged)

    // Section button state - Qt 6.8 BINDABLE Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, QVariantList, m_sectionButtonState, &FormGPS::sectionButtonStateChanged)

    // Vehicle State (6) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_speedKph, &FormGPS::speedKphChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_fusedHeading, &FormGPS::fusedHeadingChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_toolEasting, &FormGPS::toolEastingChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_toolNorthing, &FormGPS::toolNorthingChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_toolHeading, &FormGPS::toolHeadingChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, short int, m_offlineDistance, &FormGPS::offlineDistanceChanged)
    // avgPivDistance: use existing variable at line 690
    // isReverseWithIMU: use existing variable at line 527

    // Steering Control (6) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_steerAngleActual, &FormGPS::steerAngleActualChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_steerAngleSet, &FormGPS::steerAngleSetChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_lblPWMDisplay, &FormGPS::lblPWMDisplayChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_calcSteerAngleInner, &FormGPS::calcSteerAngleInnerChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_calcSteerAngleOuter, &FormGPS::calcSteerAngleOuterChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_diameter, &FormGPS::diameterChanged)

    // IMU Data (5) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_imuRoll, &FormGPS::imuRollChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_imuPitch, &FormGPS::imuPitchChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_imuHeading, &FormGPS::imuHeadingChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_imuRollDegrees, &FormGPS::imuRollDegreesChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_imuAngVel, &FormGPS::imuAngVelChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_yawRate, &FormGPS::yawRateChanged)

    // GPS Status (6) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_hdop, &FormGPS::hdopChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_age, &FormGPS::ageChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_fixQuality, &FormGPS::fixQualityChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_satellitesTracked, &FormGPS::satellitesTrackedChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_hz, &FormGPS::hzChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_rawHz, &FormGPS::rawHzChanged)
    // frameTime: use existing variable at line 242
    // steerModuleConnectedCounter: use existing variable at line 689

    // Blockage Sensors (8) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_blockage_avg, &FormGPS::blockage_avgChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_blockage_min1, &FormGPS::blockage_min1Changed)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_blockage_min2, &FormGPS::blockage_min2Changed)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_blockage_max, &FormGPS::blockage_maxChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_blockage_min1_i, &FormGPS::blockage_min1_iChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_blockage_min2_i, &FormGPS::blockage_min2_iChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_blockage_max_i, &FormGPS::blockage_max_iChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_blockage_blocked, &FormGPS::blockage_blockedChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_avgPivDistance, &FormGPS::avgPivDistanceChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_frameTime, &FormGPS::frameTimeChanged)

    // Navigation (7) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_distancePivotToTurnLine, &FormGPS::distancePivotToTurnLineChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isYouTurnRight, &FormGPS::isYouTurnRightChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isYouTurnTriggered, &FormGPS::isYouTurnTriggeredChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_current_trackNum, &FormGPS::current_trackNumChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_track_idx, &FormGPS::track_idxChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_lblmodeActualXTE, &FormGPS::lblmodeActualXTEChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_lblmodeActualHeadingError, &FormGPS::lblmodeActualHeadingErrorChanged)

    // Tool Position (2) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_toolLatitude, &FormGPS::toolLatitudeChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_toolLongitude, &FormGPS::toolLongitudeChanged)

    // Wizard/Calibration (4) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_sampleCount, &FormGPS::sampleCountChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_confidenceLevel, &FormGPS::confidenceLevelChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_hasValidRecommendation, &FormGPS::hasValidRecommendationChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_startSA, &FormGPS::startSAChanged)

    // Visual Geometry (2) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, QVariant, m_vehicle_xy, &FormGPS::vehicle_xyChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, QVariant, m_vehicle_bounding_box, &FormGPS::vehicle_bounding_boxChanged)

    // Misc Status (2) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_steerSwitchHigh, &FormGPS::steerSwitchHighChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_imuCorrected, &FormGPS::imuCorrectedChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, QString, m_lblCalcSteerAngleInner, &FormGPS::lblCalcSteerAngleInnerChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, QString, m_lblDiameter, &FormGPS::lblDiameterChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_droppedSentences, &FormGPS::droppedSentencesChanged)

    // Field Data (11) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_areaOuterBoundary, &FormGPS::areaOuterBoundaryChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_areaBoundaryOuterLessInner, &FormGPS::areaBoundaryOuterLessInnerChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_workedAreaTotal, &FormGPS::workedAreaTotalChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_workedAreaTotalUser, &FormGPS::workedAreaTotalUserChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_distanceUser, &FormGPS::distanceUserChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_actualAreaCovered, &FormGPS::actualAreaCoveredChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_userSquareMetersAlarm, &FormGPS::userSquareMetersAlarmChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isContourBtnOn, &FormGPS::isContourBtnOnChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isYouTurnBtnOn, &FormGPS::isYouTurnBtnOnChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_sensorData, &FormGPS::sensorDataChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_btnIsContourLocked, &FormGPS::btnIsContourLockedChanged)

    // Additional AOG Properties (9) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_latStart, &FormGPS::latStartChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_lonStart, &FormGPS::lonStartChanged)
    double m_mPerDegreeLat = 0.0;  // Phase 6.0.20 Task 24 Step 3.5: Simple member (no BINDABLE - C++ only)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, uint, m_sentenceCounter, &FormGPS::sentenceCounterChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_gpsHeading, &FormGPS::gpsHeadingChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isReverseWithIMU, &FormGPS::isReverseWithIMUChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_steerModuleConnectedCounter, &FormGPS::steerModuleConnectedCounterChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_autoBtnState, &FormGPS::autoBtnStateChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_manualBtnState, &FormGPS::manualBtnStateChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_autoTrackBtnState, &FormGPS::autoTrackBtnStateChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_autoYouturnBtnState, &FormGPS::autoYouturnBtnStateChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isPatchesChangingColor, &FormGPS::isPatchesChangingColorChanged)

    // Boundary Properties (4) - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isOutOfBounds, &FormGPS::isOutOfBoundsChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isHeadlandOn, &FormGPS::isHeadlandOnChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_createBndOffset, &FormGPS::createBndOffsetChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isDrawRightSide, &FormGPS::isDrawRightSideChanged)

    // RecordedPath Properties - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_isDrivingRecordedPath, &FormGPS::isDrivingRecordedPathChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, QString, m_recordedPathName, &FormGPS::recordedPathNameChanged)

    // Boundary State Properties - Qt 6.8 Rectangle Pattern
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, bool, m_boundaryIsRecording, &FormGPS::boundaryIsRecordingChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_boundaryArea, &FormGPS::boundaryAreaChanged)
    Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_boundaryPointCount, &FormGPS::boundaryPointCountChanged)


public:
    // ===== MODE DETECTION SYSTEM - TASK 6.3.0.4 =====
    // Consolidated mode detection methods for consistent operation
    bool isSimulatorMode() const { return timerSim.isActive(); }
    bool isAgIOActive() const { return m_agioService != nullptr; }
    bool isRealMode() const { return !isSimulatorMode() && isAgIOActive(); }
    bool isMixedMode() const { return isSimulatorMode() && isAgIOActive(); }

    // Operational mode for NTRIP compatibility
    bool isNTRIPCompatible() const { return isRealMode() || isMixedMode(); }
    // Публичные свойства was wizard
    bool IsCollectingData = false;
    int SampleCount = 0;
    double RecommendedWASZero = 0;
    double ConfidenceLevel = 0;
    bool HasValidRecommendation = false;
    QDateTime LastCollectionTime;

    // Средние показатели распределения
    double Mean;
    double StandardDeviation;
    double Median;

    // Методы для работы с данными

    int GetRecommendedWASOffsetAdjustment(int currentCPD);

protected:
    // Вектор для хранения истории углов
    QVector<double> steerAngleHistory;

    // Критерии проверки собираемых данных
    static constexpr int MAX_SAMPLES = 2000;          // Максимальное число хранимых образцов
    static constexpr int MIN_SAMPLES_FOR_ANALYSIS = 200; // Минимальное число образцов для анализа
    static constexpr double MIN_SPEED_THRESHOLD = 2.0;  // км/ч — минимальная скорость для начала записи
    static constexpr double MAX_ANGLE_THRESHOLD = 25.0; // Градусы — максимальный угол для включения записи

    // Личные вспомогательные методы
    bool ShouldCollectSample(double steerAngle, double speed);
    void PerformStatisticalAnalysis();
    double CalculateMedian(QVector<double> sortedData);
    double CalculateStandardDeviation(QVector<double> data, double mean);
    void CalculateConfidenceLevel(QVector<double> sortedData);
    void AddSteerAngleSample(double guidanceSteerAngle, double currentSpeed);

public slots:
    void StartDataCollection();
    void StopDataCollection();
    void ResetData();
    void ApplyOffsetToCollectedData(double appliedOffsetDegrees);
    void SmartCalLabelClick();
    void on_btnSmartZeroWAS_clicked();



};

#endif // FORMGPS_H
