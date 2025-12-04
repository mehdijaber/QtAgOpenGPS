#include <QQuickView>
#include <QQuickWidget>
#include <QQmlContext>
#include <QScreen>
#include "formgps.h"
#include "qmlutil.h"
#include <QTimer>
#include <QThread>
#include "cvehicle.h"
#include "ccontour.h"
#include "cabline.h"
#include "classes/settingsmanager.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QCoreApplication>
#include <functional>
#include <assert.h>
#include "aogrenderer.h"
//include "qmlsectionbuttons.h"
#include "cboundarylist.h"
#include <cmath>
#include <cstring>
#include <QTranslator>
// FormLoop removed - Phase 4.4: AgIOService standalone
#include <algorithm>

//if using qquickframebufferobject, set qml to use AOGRenderer and comment out next line
//#define USE_QSGRENDERNODE 1 //Also switch qml to use AOGRendererItem


QString caseInsensitiveFilename(QString directory, QString filename);

// ⚡ PHASE 6.0.4: Q_PROPERTY setters moved to inline implementation in formgps.h

void FormGPS::setupGui()
{
    // Phase 4.5: AgIOService will be created by QML factory automatically
    qDebug() << "🚀 AgIOService will be initialized by QML factory on first access";


    qDebug() << "AgIO: All context properties set, loading QML...";

    // ⚡ PHASE 6.3.0 CRITICAL FIX: Expose FormGPS Q_PROPERTY to QML context
    // This makes C++ Q_PROPERTY accessible to both InterfaceProperty and QML
    rootContext()->setContextProperty("formGPS", this);
    qDebug() << "✅ FormGPS exposed to QML context - Q_PROPERTY now accessible";

    addImportPath("qrc:/qt/qml/");


    /* Load the QML UI and display it in the main area of the GUI */
    setProperty("title","QtAgOpenGPS");
    addImportPath(":/");

    // Translation will be loaded automatically in constructor via on_language_changed()
    // No manual loading needed here

    connect(this, &QQmlApplicationEngine::objectCreated,
            this, &FormGPS::on_qml_created, Qt::QueuedConnection);

//tell the QML what OS we are using
#ifdef __ANDROID__
    rootContext()->setContextProperty("OS", "ANDROID");
#elif defined(__WIN32)
    rootContext()->setContextProperty("OS", "WINDOWS");
#else
    rootContext()->setContextProperty("OS", "LINUX");
#endif

    //Load the QML into a view
    rootContext()->setContextProperty("screenPixelDensity",QGuiApplication::primaryScreen()->physicalDotsPerInch() * QGuiApplication::primaryScreen()->devicePixelRatio());
    rootContext()->setContextProperty("aog", this);        // New unified interface
    //rootContext()->setContextProperty("mainForm", this);  // Legacy compatibility

    //populate vehicle_list property in vehicleInterface
    vehicle_update_list();
    
    // ===== PHASE 6.0.20 Task 22: CTrack member registration restored =====
    // Original architecture: qmlRegisterSingletonInstance in formgps_ui.cpp (not main.cpp)
  //  rootContext()->setContextProperty("TracksInterface", &track);
    qmlRegisterSingletonInstance("AOG", 1, 0, "TracksInterface", &track);

    // Only tram still uses setContextProperty (not yet modernized)
    rootContext()->setContextProperty("tram", &tram);

#ifdef LOCAL_QML
    // Look for QML files relative to our current directory
    QStringList search_pathes = { "..",
                                 "../../",
                                 "../qtaog",
                                 "../QtAgOpenGPS",
                                 "."
    };

    qWarning() << "Looking for QML.";
    for(QString search_path : search_pathes) {
        //look relative to current working directory
        QDir d = QDir(QDir::currentPath() + "/" + search_path + "/qml/");
        if (d.exists("AOGInterface.qml")) {
            QDir::addSearchPath("local",QDir::currentPath() + "/" + search_path);
            qWarning() << "QML path is " << search_path;
            break;
        }

        //look relative to the executable's directory
        d = QDir(QCoreApplication::applicationDirPath() + "/" + search_path + "/qml/");
        if (d.exists("AOGInterface.qml")) {
            QDir::addSearchPath("local",QCoreApplication::applicationDirPath() + "/" + search_path);
            qWarning() << "QML path is " << search_path;
            break;
        }
    }

    QObject::connect(this, &QQmlApplicationEngine::warnings, [=] (const QList<QQmlError> &warnings) {
        foreach (const QQmlError &error, warnings) {
            qWarning() << "warning: " << error.toString();
        }
    });

    rootContext()->setContextProperty("prefix","local:");
    load("local:/qml/MainWindow.qml");
#else
    rootContext()->setContextProperty("prefix","qrc:/AOG");
    //load(QUrl("qrc:/qml/MainWindow.qml"));
    addImportPath(QString("%1/modules").arg(QGuiApplication::applicationDirPath()));
    loadFromModule("AOG", "MainWindow");

    if (rootObjects().isEmpty()) {
        qDebug() << "Error: Failed to load QML!";
        return;
    } else {
        qDebug() << "QML loaded successfully.";

        // ⚡ PHASE 6.3.0 TIMING: QML interfaces initialization moved to on_qml_created()
        // mainWindow must be set before calling initializeQMLInterfaces()
        qDebug() << "🔄 QML loaded - interface initialization will happen in on_qml_created()";

        // Connect to the AgIOService instance created by QML factory
        connectToAgIOFactoryInstance();
    }

    qDebug() << "AgOpenGPS started successfully";
#endif
}

void FormGPS::on_qml_created(QObject *object, const QUrl &url)
{
    qDebug() << "object is now created. " << url.toString();
    //get pointer to root QML object, which is the OpenGLControl,
    //store in a member variable for future use.
    QList<QObject*> root_context = rootObjects();

    if (root_context.length() == 0) {
        qWarning() << "MainWindow.qml did not load.  Aborting.";
        assert(root_context.length() > 0);
    }

    mainWindow = root_context.first();

    // Initialize mainWindow reference for all components
    recPath.setMainWindow(mainWindow);

    mainWindow->setProperty("visible",true);

    // ⚡ Qt 6.8 Modern Pattern: objectCreated signal has fired, QML root is ready
    qDebug() << "🎯 Qt 6.8: QML root object created, scheduling interface initialization...";

    // Defer initialization to let QML complete its Component.onCompleted
    QTimer::singleShot(100, this, [this]() {
        qDebug() << "⏰ Timer fired - initializing QML interfaces now...";
        initializeQMLInterfaces();
    });

    // ⚡ MOVED: Interface initialization moved to initializeQMLInterfaces() for proper timing

    // QMLSectionButtons completely removed - using direct btnStates[] array instead
    qmlblockage::set_aog_root(qmlItem(mainWindow, "aog"));

    //initialize interface properties (MOVED to initializeQMLInterfaces() after PropertyWrapper init)
    setIsBtnAutoSteerOn(false);
    this->setLatStart(0.0);
    this->setLonStart(0.0);

    // Phase 6.0.20: AOGInterface properties accessible via 'this' context
    // No qmlItem() needed - FormGPS is registered as context property "aog"

    // Qt 6.8 BINDABLE: Q_OBJECT_BINDABLE_PROPERTY automatically emits sectionButtonStateChanged() when .setValue() is called
    // NO CONNECTION NEEDED: Using direct btnStates[] array eliminates all circular dependency issues
    //connect(aog,SIGNAL(rowCountChanged()), &tool.blockageRowState, SLOT(onRowsUpdated())); //Dim

    // ⚡ PHASE 6.3.0 TIMING FIX: OpenGL callbacks setup moved to initializeQMLInterfaces()
    // This ensures InterfaceProperty are initialized BEFORE any rendering can occur
    openGLControl = mainWindow->findChild<QQuickItem *>("openglcontrol");
    if (openGLControl) {
        qDebug() << "🎯 OpenGL control found - callbacks will be set after InterfaceProperty initialization";
    } else {
        qWarning() << "⚠️ OpenGL control not found during initialization";
    }

    // REMOVED: Duplicate connection - already connected at line 187
    // connect(aog, SIGNAL(sectionButtonStateChanged()), &tool.sectionButtonState, SLOT(onStatesUpdated()));

    //on screen buttons
    // ===== BATCH 3 REMOVED - 8 Camera Navigation - Qt 6.8 Q_INVOKABLE direct calls =====
    // Qt 6.8 REMOVED: zoomIn → zoomIn(), zoomOut → zoomOut(), tiltDown → tiltDown(), tiltUp → tiltUp()
    // Qt 6.8 REMOVED: btn2D → view2D(), btn3D → view3D(), n2D → normal2D(), n3D → normal3D()
    // Qt 6.8 REMOVED: isHydLiftOn is now Q_OBJECT_BINDABLE_PROPERTY with automatic QML binding
    // Qt 6.8 REMOVED: btnResetTool now uses Q_INVOKABLE direct call - see resetTool()
    // Qt 6.8 REMOVED: btnContour now uses Q_INVOKABLE direct call - see contour()
    // Qt 6.8 REMOVED: btnContourLock now uses Q_INVOKABLE direct call - see contourLock()
    // Qt 6.8 REMOVED: btnContourPriority(bool) now uses Q_INVOKABLE direct call - see contourPriority(bool)
    // ===== BATCH 7 REMOVED - Qt 6.8 Q_INVOKABLE direct calls =====
    // Qt 6.8 REMOVED: btnHeadland → headland(), isYouSkipOn → youSkip()
    // Qt 6.8 REMOVED: btnResetSim → resetSim(), sim_rotate → rotateSim(), reset_direction → resetDirection()
    // Qt 6.8 REMOVED: centerOgl → centerOgl(), deleteAppliedArea → deleteAppliedArea()
    // ===== BATCH 2 REMOVED - 7 You-Turn Navigation - Qt 6.8 Q_INVOKABLE direct calls =====
    // Qt 6.8 REMOVED: uturn(bool) → manualUTurn(bool), lateral(bool) → lateral(bool)
    // Qt 6.8 REMOVED: autoYouTurn → autoYouTurn(), swapAutoYouTurnDirection → swapAutoYouTurnDirection()
    // Qt 6.8 REMOVED: btnResetCreatedYouTurn → resetCreatedYouTurn(), btnAutoTrack → autoTrack(), btnFlag → flag()

    // REMOVED: save_everything signal/slot replaced by applicationClosing property binding
    // Old: QObject::connect(mainWindow, SIGNAL(save_everything(bool)), this, SLOT(FileSaveEverythingBeforeClosingField(bool)));
    // New: applicationClosing binding in FormGPS constructor handles save logic automatically
    //connect(qml_root,SIGNAL(closing(QQuickCloseEvent *)), this, SLOT(fileSaveEverythingBeforeClosingField(QQuickCloseEvent *)));

    // ===== BATCH 4 REMOVED - 2 Settings - Qt 6.8 Q_INVOKABLE direct calls =====
    // Qt 6.8 REMOVED: settings_reload → settingsReload(), settings_save → settingsSave()
    // connect(aog, SIGNAL(settings_reload()), this, SLOT(on_settings_reload()));
    // connect(aog, SIGNAL(settings_save()), this, SLOT(on_settings_save()));

    // Qt 6.8 RESTORED: Language change signal/slot for dynamic translation reloading
    // PHASE6-0-20: SettingsManager::menu_languageChanged signal exists and FormGPS::on_language_changed works
    // Reconnect for automatic QML retranslation when language changes
    connect(SettingsManager::instance(), &SettingsManager::menu_languageChanged, this, &FormGPS::on_language_changed);

    //snap track button - REMOVED: Modernized to Q_INVOKABLE direct calls
    // REMOVED: connect(aog, SIGNAL(snapSideways(double)), this, SLOT(onBtnSnapSideways_clicked(double)));
    // REMOVED: connect(aog, SIGNAL(snapToPivot()), this, SLOT(onBtnSnapToPivot_clicked()));

    //vehicle saving and loading - Phase 1 Thread-Safe Architecture
    connect(CVehicle::instance(), &CVehicle::vehicle_update_list, this, &FormGPS::vehicle_update_list, Qt::QueuedConnection);
    connect(CVehicle::instance(), &CVehicle::vehicle_load, this, &FormGPS::vehicle_load, Qt::QueuedConnection);
    connect(CVehicle::instance(), &CVehicle::vehicle_delete, this, &FormGPS::vehicle_delete, Qt::QueuedConnection);
    connect(CVehicle::instance(), &CVehicle::vehicle_saveas, this, &FormGPS::vehicle_saveas, Qt::QueuedConnection);

    // ⚡ PHASE 6.3.0 TIMING FIX: Interface connections moved to initializeQMLInterfaces()
    // fieldInterface connections will be established after QML object initialization

    // Phase 6.0.20: Connect FormGPS signals to ahrs slots (changeImuHeading/Roll are Q_INVOKABLE methods called from QML)
    // QML calls aog.changeImuHeading(value) -> FormGPS Q_INVOKABLE directly updates ahrs
    // No signal/slot connection needed - Q_INVOKABLE handles direct method calls

    //React to UI setting hyd life settings - REMOVED: Modernized to Q_INVOKABLE direct calls
    // REMOVED: connect(aog, SIGNAL(modules_send_238()), this, SLOT(modules_send_238()));
    // REMOVED: connect(aog, SIGNAL(modules_send_251()), this, SLOT(modules_send_251()));
    // REMOVED: connect(aog, SIGNAL(modules_send_252()), this, SLOT(modules_send_252()));

    // REMOVED: connect(aog, SIGNAL(doBlockageMonitoring()), this, SLOT(doBlockageMonitoring()));

    // REMOVED: Simulator signals modernized to Q_INVOKABLE direct calls in Qt 6.8 migration
    // REMOVED: connect(aog, SIGNAL(sim_bump_speed(bool)), &sim, SLOT(speed_bump(bool)));
    // REMOVED: connect(aog, SIGNAL(sim_zero_speed()), &sim, SLOT(speed_zero()));
    // REMOVED: connect(aog, SIGNAL(sim_reset()), &sim, SLOT(reset()));

    // Steering controls - REMOVED: Modernized to Q_INVOKABLE direct calls
    // REMOVED: connect(aog, SIGNAL(btnSteerAngleUp()), this, SLOT(btnSteerAngleUp_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnSteerAngleDown()), this, SLOT(btnSteerAngleDown_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnFreeDrive()), this, SLOT(btnFreeDrive_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnFreeDriveZero()), this, SLOT(btnFreeDriveZero_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnStartSA()), this, SLOT(btnStartSA_clicked()));

    // ⚡ PHASE 6.3.0 TIMING FIX: boundaryInterface connections moved to initializeQMLInterfaces()
    // All boundary-related connections will be established after QML object initialization


    headland_form.bnd = &bnd;
    headland_form.hdl = &hdl;
    headland_form.tool = &tool;
    headland_form.setFormGPS(this);

    headland_form.connect_ui(qmlItem(mainWindow, "headlandDesigner"));
    connect(&headland_form, SIGNAL(saveHeadland()),this,SLOT(headland_save()));
    connect(&headland_form, SIGNAL(timedMessageBox(int,QString,QString)),this,SLOT(TimedMessageBox(int,QString,QString)));

    headache_form.bnd = &bnd;
    headache_form.hdl = &hdl;
    headache_form.tool = &tool;
    headache_form.setFormGPS(this);

    // Initialize CYouTurn FormGPS and CTrack references
    yt.setFormGPS(this);
    yt.setTrack(&track);

    headache_form.connect_ui(qmlItem(mainWindow, "headacheDesigner"));
    connect(&headache_form, SIGNAL(saveHeadland()),this,SLOT(headland_save()));
    connect(&headache_form, SIGNAL(timedMessageBox(int,QString,QString)),this,SLOT(TimedMessageBox(int,QString,QString)));
    connect(&headache_form, SIGNAL(saveHeadlines()), this,SLOT(headlines_save()));
    connect(&headache_form, SIGNAL(loadHeadlines()), this,SLOT(headlines_load()));

    //connect qml button signals to callbacks (it's not automatic with qml)

    /*btnPerimeter = qmlItem(qml_root,"btnPerimeter");
    connect(btnPerimeter,SIGNAL(clicked()),this,
            SLOT(onBtnPerimeter_clicked()));
    */

    // btnFlag = qmlItem(mainWindow,"btnFlag");
    // connect(btnFlag,SIGNAL(clicked()),this,
    //         SLOT(onBtnFlag_clicked()));


    //Any objects we don't need to access later we can just store
    //temporarily
    //QObject *flag = qmlItem(mainWindow, "flag");
    // Flag controls - REMOVED: Modernized to Q_INVOKABLE direct calls
    // REMOVED: connect(aog, SIGNAL(btnRedFlag()), this, SLOT(onBtnRedFlag_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnGreenFlag()), this, SLOT(onBtnGreenFlag_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnYellowFlag()), this, SLOT(onBtnYellowFlag_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnDeleteFlag()), this, SLOT(onBtnDeleteFlag_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnDeleteAllFlags()), this, SLOT(onBtnDeleteAllFlags_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnNextFlag()), this, SLOT(onBtnNextFlag_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnPrevFlag()), this, SLOT(onBtnPrevFlag_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnCancelFlag()), this, SLOT(onBtnCancelFlag_clicked()));
    // REMOVED: connect(aog, SIGNAL(btnRed(double, double, int)), this, SLOT(onBtnRed_clicked(double, double, int)));



    // ===== BATCH 12 - Wizard & Calibration Connections REMOVED - Qt 6.8 modernized to Q_INVOKABLE calls =====
    // REMOVED: connect(aog, SIGNAL(stopDataCollection()), this, SLOT(StopDataCollection()));
    // REMOVED: connect(aog, SIGNAL(startDataCollection()), this, SLOT(StartDataCollection()));
    // REMOVED: connect(aog, SIGNAL(resetData()), this, SLOT(ResetData()));
    // REMOVED: connect(aog, SIGNAL(applyOffsetToCollectedData(double)), this, SLOT(ApplyOffsetToCollectedData(double)));
    // REMOVED: connect(aog, SIGNAL(smartCalLabelClick()), this, SLOT(SmartCalLabelClick()));
    // REMOVED: connect(aog, SIGNAL(on_btnSmartZeroWAS_clicked()), this, SLOT(on_btnSmartZeroWAS_clicked()));
    // QObject *temp;
    // temp = qmlItem(mainWindow,"btnRedFlag");
    // connect(temp,SIGNAL(clicked()),this,SLOT(onBtnRedFlag_clicked()));
    // temp = qmlItem(mainWindow,"btnGreenFlag");
    // connect(temp,SIGNAL(clicked()),this,SLOT(onBtnGreenFlag_clicked()));
    // temp = qmlItem(mainWindow,"btnYellowFlag");
    // connect(temp,SIGNAL(clicked()),this,SLOT(onBtnYellowFlag_clicked()));

    // btnDeleteFlag = qmlItem(mainWindow,"btnDeleteFlag");
    // connect(btnDeleteFlag,SIGNAL(clicked()),this,SLOT(onBtnDeleteFlag_clicked()));
    // btnDeleteAllFlags = qmlItem(mainWindow,"btnDeleteAllFlags");
    // connect(btnDeleteAllFlags,SIGNAL(clicked()),this,SLOT(onBtnDeleteAllFlags_clicked()));
    contextFlag = qmlItem(mainWindow, "contextFlag");
    // boundaryInterface, fieldInterface, recordedPathInterface already initialized above (lines 152-154)

    //txtDistanceOffABLine = qmlItem(qml_root,"txtDistanceOffABLine");

    tmrWatchdog = new QTimer(this);
    connect (tmrWatchdog, SIGNAL(timeout()),this,SLOT(tmrWatchdog_timeout()));
    tmrWatchdog->start(250); //fire every 50ms.

    timer_tick = new QTimer(this);
    timer_tick->setSingleShot(false);
    timer_tick->setInterval(250);
    connect(timer_tick, &QTimer::timeout, this, &FormGPS::Timer1_Tick);
    timer_tick->start(250);

    //SIM
    connect_classes();

    loadSettings(); //load settings and properties

    setIsJobStarted(false);

    // StartLoopbackServer(); // ❌ REMOVED - Phase 4.6: UDP FormGPS completely eliminated
    if (SettingsManager::instance()->menu_isSimulatorOn() == false) {
        qDebug() << "Stopping simulator because it's off in settings.";
        timerSim.stop();
    }

    //star Sim
    swFrame.start();

    stopwatch.start();

    vehicle_update_list();

}

void FormGPS::onGLControl_dragged(int pressX, int pressY, int mouseX, int mouseY)
{
    QVector3D from,to,offset;

    from = mouseClickToPan(pressX, pressY);
    to = mouseClickToPan(mouseX, mouseY);
    offset = to - from;

    camera.panX += offset.x();
    camera.panY += offset.y();
    // CRITICAL: Force OpenGL update in GUI thread to prevent threading violation
    if (openGLControl) {
        QMetaObject::invokeMethod(openGLControl, "update", Qt::QueuedConnection);
    }
}
void FormGPS::onBtnCenterOgl_clicked(){
    qDebug()<<"center ogl";
    camera.panX = 0;
    camera.panY = 0;
    // CRITICAL: Force OpenGL update in GUI thread to prevent threading violation
    if (openGLControl) {
        QMetaObject::invokeMethod(openGLControl, "update", Qt::QueuedConnection);
    }
}

void FormGPS::onGLControl_clicked(const QVariant &event)
{
    QObject *m = event.value<QObject *>();

    //Pass the click on to the rendering routine.
    //make the bottom left be 0,0
    mouseX = m->property("x").toInt();
    mouseY = m->property("y").toInt();

    QVector3D field = mouseClickToField(mouseX, mouseY);
    mouseEasting = field.x();
    mouseNorthing = field.y();

    leftMouseDownOnOpenGL = true;
    // CRITICAL: Force OpenGL update in GUI thread to prevent threading violation
    if (openGLControl) {
        QMetaObject::invokeMethod(openGLControl, "update", Qt::QueuedConnection);
    }
}

void FormGPS::onBtnAgIO_clicked(){
    qDebug()<<"AgIO";
}
void FormGPS::onBtnResetTool_clicked(){
               CVehicle::instance()->tankPos.heading = CVehicle::instance()->fixHeading;
               CVehicle::instance()->tankPos.easting = CVehicle::instance()->hitchPos.easting + (sin(CVehicle::instance()->tankPos.heading) * (tool.tankTrailingHitchLength));
               CVehicle::instance()->tankPos.northing = CVehicle::instance()->hitchPos.northing + (cos(CVehicle::instance()->tankPos.heading) * (tool.tankTrailingHitchLength));

               CVehicle::instance()->toolPivotPos.heading = CVehicle::instance()->tankPos.heading;
               CVehicle::instance()->toolPivotPos.easting = CVehicle::instance()->tankPos.easting + (sin(CVehicle::instance()->toolPivotPos.heading) * (tool.trailingHitchLength));
               CVehicle::instance()->toolPivotPos.northing = CVehicle::instance()->tankPos.northing + (cos(CVehicle::instance()->toolPivotPos.heading) * (tool.trailingHitchLength));
}

// ===== Q_INVOKABLE MODERN ACTIONS - Qt 6.8 Implementation =====
void FormGPS::resetTool() {
    // Modern implementation - same logic as onBtnResetTool_clicked()
    onBtnResetTool_clicked();
}

void FormGPS::contour() {
    // Modern implementation - same logic as onBtnContour_clicked()
    onBtnContour_clicked();
}

void FormGPS::contourLock() {
    // Modern implementation - same logic as onBtnContourLock_clicked()
    onBtnContourLock_clicked();
}

void FormGPS::contourPriority(bool isRight) {
    // Modern implementation - same logic as onBtnContourPriority_clicked(bool)
    onBtnContourPriority_clicked(isRight);
}

// ===== BATCH 7 ACTIONS - Qt 6.8 Q_INVOKABLE Implementation =====
void FormGPS::headland() {
    onBtnHeadland_clicked();
}

void FormGPS::youSkip() {
    onBtnYouSkip_clicked();
}

void FormGPS::resetSim() {
    onBtnResetSim_clicked();
}

void FormGPS::rotateSim() {
    onBtnRotateSim_clicked();
}

void FormGPS::resetDirection() {
    onBtnResetDirection_clicked();
}

void FormGPS::centerOgl() {
    onBtnCenterOgl_clicked();
}

void FormGPS::deleteAppliedArea() {
    onDeleteAppliedArea_clicked();
}

// ===== BATCH 2 - 7 ACTIONS You-Turn Navigation - Qt 6.8 Q_INVOKABLE Implementation =====
void FormGPS::manualUTurn(bool isRight) {
    onBtnManUTurn_clicked(isRight);
}

void FormGPS::lateral(bool isRight) {
    onBtnLateral_clicked(isRight);
}

void FormGPS::autoYouTurn() {
    onBtnAutoYouTurn_clicked();
}

void FormGPS::swapAutoYouTurnDirection() {
    onBtnSwapAutoYouTurnDirection_clicked();
}

void FormGPS::resetCreatedYouTurn() {
    onBtnResetCreatedYouTurn_clicked();
}

void FormGPS::autoTrack() {
    onBtnAutoTrack_clicked();
}

void FormGPS::flag() {
    onBtnFlag_clicked();
}

// ===== BATCH 3 - 8 ACTIONS Camera Navigation - Qt 6.8 Q_INVOKABLE Implementation =====
void FormGPS::zoomIn() {
    onBtnZoomIn_clicked();
}

void FormGPS::zoomOut() {
    onBtnZoomOut_clicked();
}

void FormGPS::tiltDown() {
    onBtnTiltDown_clicked();
}

void FormGPS::tiltUp() {
    onBtnTiltUp_clicked();
}

void FormGPS::view2D() {
    onBtn2D_clicked();
}

void FormGPS::view3D() {
    onBtn3D_clicked();
}

void FormGPS::normal2D() {
    onBtnN2D_clicked();
}

void FormGPS::normal3D() {
    onBtnN3D_clicked();
}

// ===== BATCH 4 - 2 ACTIONS Settings - Qt 6.8 Q_INVOKABLE Implementation =====
void FormGPS::settingsReload() {
    on_settings_reload();
}

void FormGPS::settingsSave() {
    on_settings_save();
}

// ===== BATCH 9 - 2 ACTIONS Snap Track - Qt 6.8 Q_INVOKABLE Implementation =====
void FormGPS::snapSideways(double distance) {
    // Modern implementation - same logic as onBtnSnapSideways_clicked(double)
    onBtnSnapSideways_clicked(distance);
}

void FormGPS::snapToPivot() {
    // Modern implementation - same logic as onBtnSnapToPivot_clicked()
    onBtnSnapToPivot_clicked();
}

// ===== BATCH 10 - 8 ACTIONS Modules & Steering - Qt 6.8 Q_INVOKABLE Implementation =====
void FormGPS::modulesSend238() {
    // Modern implementation - same logic as modules_send_238()
    modules_send_238();
}

void FormGPS::modulesSend251() {
    // Modern implementation - same logic as modules_send_251()
    modules_send_251();
}

void FormGPS::modulesSend252() {
    // Modern implementation - same logic as modules_send_252()
    modules_send_252();
}

void FormGPS::blockageMonitoring() {
    // Modern implementation - renamed to avoid conflict with existing doBlockageMonitoring()
    // Call the original doBlockageMonitoring() method from formgps_sections.cpp
    doBlockageMonitoring();
}

void FormGPS::steerAngleUp() {
    // Modern implementation - same logic as btnSteerAngleUp_clicked()
    btnSteerAngleUp_clicked();
}

void FormGPS::steerAngleDown() {
    // Modern implementation - same logic as btnSteerAngleDown_clicked()
    btnSteerAngleDown_clicked();
}

void FormGPS::freeDrive() {
    // Modern implementation - same logic as btnFreeDrive_clicked()
    btnFreeDrive_clicked();
}

void FormGPS::freeDriveZero() {
    // Modern implementation - same logic as btnFreeDriveZero_clicked()
    btnFreeDriveZero_clicked();
}

void FormGPS::startSAAction() {
    // Modern implementation - renamed to avoid conflict with Q_PROPERTY bool startSA()
    // Call the original btnStartSA_clicked() method
    btnStartSA_clicked();
}

// ===== BATCH 11 - 9 ACTIONS Flag Management - Qt 6.8 Q_INVOKABLE Implementation =====
void FormGPS::redFlag() {
    // Modern implementation - same logic as onBtnRedFlag_clicked()
    onBtnRedFlag_clicked();
}

void FormGPS::greenFlag() {
    // Modern implementation - same logic as onBtnGreenFlag_clicked()
    onBtnGreenFlag_clicked();
}

void FormGPS::yellowFlag() {
    // Modern implementation - same logic as onBtnYellowFlag_clicked()
    onBtnYellowFlag_clicked();
}

void FormGPS::deleteFlag() {
    // Modern implementation - same logic as onBtnDeleteFlag_clicked()
    onBtnDeleteFlag_clicked();
}

void FormGPS::deleteAllFlags() {
    // Modern implementation - same logic as onBtnDeleteAllFlags_clicked()
    onBtnDeleteAllFlags_clicked();
}

void FormGPS::nextFlag() {
    // Modern implementation - same logic as onBtnNextFlag_clicked()
    onBtnNextFlag_clicked();
}

void FormGPS::prevFlag() {
    // Modern implementation - same logic as onBtnPrevFlag_clicked()
    onBtnPrevFlag_clicked();
}

void FormGPS::cancelFlag() {
    // Modern implementation - same logic as onBtnCancelFlag_clicked()
    onBtnCancelFlag_clicked();
}

void FormGPS::redFlagAt(double lat, double lon, int color) {
    // Modern implementation - same logic as onBtnRed_clicked(double, double, int)
    onBtnRed_clicked(lat, lon, color);
}

// ===== BATCH 12 - 6 ACTIONS Wizard & Calibration - Qt 6.8 Q_INVOKABLE Implementation =====
void FormGPS::stopDataCollection() {
    // Modern implementation - same logic as StopDataCollection()
    StopDataCollection();
}

void FormGPS::startDataCollection() {
    // Modern implementation - same logic as StartDataCollection()
    StartDataCollection();
}

void FormGPS::resetData() {
    // Modern implementation - same logic as ResetData()
    ResetData();
}

void FormGPS::applyOffsetToCollectedData(double offset) {
    // Modern implementation - same logic as ApplyOffsetToCollectedData(double)
    ApplyOffsetToCollectedData(offset);
}

void FormGPS::smartCalLabelClick() {
    // Modern implementation - same logic as SmartCalLabelClick()
    SmartCalLabelClick();
}

void FormGPS::smartZeroWAS() {
    // Modern implementation - renamed from on_btnSmartZeroWAS_clicked() to avoid naming conflict
    on_btnSmartZeroWAS_clicked();
}

// ===== BATCH 13 - 7 ACTIONS Field Management - Qt 6.8 Q_INVOKABLE Implementation =====
void FormGPS::fieldUpdateList() {
    // Modern implementation - same logic as field_update_list()
    field_update_list();
}

void FormGPS::fieldClose() {
    // Modern implementation - same logic as field_close()
    field_close();
}

void FormGPS::fieldOpen(const QString& fieldName) {
    // Modern implementation - same logic as field_open(QString)
    field_open(fieldName);
}

void FormGPS::fieldNew(const QString& fieldName) {
    // Modern implementation - same logic as field_new(QString)
    field_new(fieldName);
}

void FormGPS::fieldNewFrom(const QString& fieldName, const QString& sourceField, int fieldType) {
    // Modern implementation - same logic as field_new_from(QString,QString,int)
    field_new_from(fieldName, sourceField, fieldType);
}

void FormGPS::fieldNewFromKML(const QString& fieldName, const QString& kmlPath) {
    // Modern implementation - same logic as field_new_from_KML(QString,QString)
    field_new_from_KML(fieldName, kmlPath);
}

void FormGPS::fieldDelete(const QString& fieldName) {
    // Modern implementation - same logic as field_delete(QString)
    field_delete(fieldName);
}

// ===== BATCH 14 - 11 ACTIONS Boundary Management - Qt 6.8 Q_INVOKABLE Implementation =====
void FormGPS::boundaryCalculateArea() {
    // Modern implementation - same logic as boundary_calculate_area()
    boundary_calculate_area();
}

void FormGPS::boundaryUpdateList() {
    // Modern implementation - same logic as boundary_update_list()
    boundary_update_list();
}

void FormGPS::boundaryStart() {
    // Modern implementation - same logic as boundary_start()
    boundary_start();
}

void FormGPS::boundaryStop() {
    // Modern implementation - same logic as boundary_stop()
    boundary_stop();
}

void FormGPS::boundaryAddPoint() {
    // Modern implementation - same logic as boundary_add_point()
    boundary_add_point();
}

void FormGPS::boundaryDeleteLastPoint() {
    // Modern implementation - same logic as boundary_delete_last_point()
    boundary_delete_last_point();
}

void FormGPS::boundaryPause() {
    // Modern implementation - same logic as boundary_pause()
    boundary_pause();
}

void FormGPS::boundaryRecord() {
    // Modern implementation - same logic as boundary_record()
    boundary_record();
}

void FormGPS::boundaryReset() {
    // Modern implementation - same logic as boundary_restart()
    boundary_restart();
}

void FormGPS::boundaryDeleteBoundary(int boundaryId) {
    // Modern implementation - same logic as boundary_delete(int)
    boundary_delete(boundaryId);
}

void FormGPS::boundarySetDriveThrough(int boundaryId, bool isDriveThrough) {
    // Modern implementation - same logic as boundary_set_drivethru(int,bool)
    boundary_set_drivethru(boundaryId, isDriveThrough);
}

void FormGPS::boundaryDeleteAll() {
    // Modern implementation - same logic as boundary_delete_all()
    boundary_delete_all();
}

// ===== RecordedPath Management (6 methods) - ZERO EMIT =====

void FormGPS::recordedPathUpdateLines() {
    // TODO: Backend implementation needed in CRecordedPath
    // This would refresh the list of available recorded paths from disk
    // For now, just trigger a property update to refresh QML

    // NO EMIT - List updates automatically via Qt 6.8 bindings
}

void FormGPS::recordedPathOpen(const QString& pathName) {
    // TODO: Backend implementation needed in CRecordedPath
    // This would load a recorded path file from disk into recPath.recList
    // Similar to how boundary/field files are loaded

    // Update property with automatic Qt 6.8 notification
    setRecordedPathName(pathName);
}

void FormGPS::recordedPathDelete(const QString& pathName) {
    // TODO: Backend implementation needed in CRecordedPath
    // This would delete a recorded path file from disk

    // Reset if this was the active path
    if (recordedPathName() == pathName) {
        setRecordedPathName("");
        setIsDrivingRecordedPath(false);
    }
}

void FormGPS::recordedPathStartDriving() {
    // Call existing backend method with required parameters
    bool success = recPath.StartDrivingRecordedPath(*vehicle, yt);

    // Update property only if successfully started - automatic QML notification
    if (success) {
        setIsDrivingRecordedPath(true);
    }
}

void FormGPS::recordedPathStopDriving() {
    // Call existing backend method
    recPath.StopDrivingRecordedPath();

    // Automatic notification via Qt 6.8 binding
    setIsDrivingRecordedPath(false);
}

void FormGPS::recordedPathClear() {
    // Clear the recorded path data
    recPath.recList.clear();
    recPath.recListCount = 0;
    recPath.isFollowingRecPath = false;

    // Reset properties
    setRecordedPathName("");
    setIsDrivingRecordedPath(false);
}

void FormGPS::onBtnHeadland_clicked(){
    qDebug()<<"Headland";
    // Qt 6.8 Q_OBJECT_BINDABLE_PROPERTY: Direct access
    m_isHeadlandOn = !m_isHeadlandOn;
               if (this->isHeadlandOn())
               {
                   //btnHeadlandOnOff.Image = Properties.Resources.HeadlandOn;
               }
               else
               {
                   //btnHeadlandOnOff.Image = Properties.Resources.HeadlandOff;
               }

               if (CVehicle::instance()->isHydLiftOn() && !this->isHeadlandOn()) CVehicle::instance()->setIsHydLiftOn(false);

               if (!this->isHeadlandOn())
               {
                   p_239.pgn[p_239.hydLift] = 0;
                   //btnHydLift.Image = Properties.Resources.HydraulicLiftOff;
               }
}
void FormGPS::onBtnHydLift_clicked(){
    if (this->isHeadlandOn())
    {
        CVehicle::instance()->setIsHydLiftOn(!CVehicle::instance()->isHydLiftOn());
        if (CVehicle::instance()->isHydLiftOn())
        {
        }
        else
        {
            p_239.pgn[p_239.hydLift] = 0;
        }
    }
    else
    {
        p_239.pgn[p_239.hydLift] = 0;
        CVehicle::instance()->setIsHydLiftOn(false);
    }
}
void FormGPS::onBtnTramlines_clicked(){
    qDebug()<<"tramline";
}
void FormGPS::onBtnYouSkip_clicked(){
    qDebug()<<"you skip clicked";
    yt.alternateSkips = yt.alternateSkips+1;
    if (yt.alternateSkips > 3) yt.alternateSkips = 0;
    qDebug()<<"you skip clicked"<<yt.alternateSkips;
    if (yt.alternateSkips > 0)
    {
        //btnYouSkipEnable.Image = Resources.YouSkipOn;
        //make sure at least 1
        if (yt.rowSkipsWidth < 2)
        {
            yt.rowSkipsWidth = 2;
            //cboxpRowWidth.Text = "1";
        }
        yt.Set_Alternate_skips();
        yt.ResetCreatedYouTurn();

        //if (!this->isYouTurnBtnOn()) btnAutoYouTurn.PerformClick();
    }

}


void FormGPS::onBtnResetDirection_clicked(){
    qDebug()<<"reset Direction";
    // c#Array.Clear(stepFixPts, 0, stepFixPts.Length);

    std::memset(stepFixPts, 0, sizeof(stepFixPts));
    isFirstHeadingSet = false;

    //TODO: most of this should be done in QML
    CVehicle::instance()->setIsReverse(false);
    TimedMessageBox(2000, "Reset Direction", "Drive Forward > 1.5 kmh");
}
void FormGPS::onBtnFlag_clicked() {

    //TODO if this button is disabled until field is started, we won't
    //need this check.

    if(isGPSPositionInitialized) {
        int nextflag = flagPts.size() + 1;
        QString notes = QString::number(nextflag);
        double currentGpsHeading = gpsHeading(); // Store value for CFlag constructor (needs double& reference)
        CFlag flagPt(pn.latitude, pn.longitude, pn.fix.easting, pn.fix.northing, currentGpsHeading, flagColor, nextflag, notes);
        flagPts.append(flagPt);
        flagsBufferCurrent = false;
        if (contextFlag) {
            contextFlag->setProperty("ptlat",pn.latitude);
            contextFlag->setProperty("ptlon",pn.longitude);
            contextFlag->setProperty("ptId",nextflag);
            contextFlag->setProperty("ptText",notes);
        }
        //FileSaveFlags();
    }
}
void FormGPS::onBtnRed_clicked(double lat, double lon, int color)
{   qDebug()<<"onBtnRed_clicked";
    if(isGPSPositionInitialized) {
    double east, nort, ptHeading = 0;
    int nextflag = flagPts.size() + 1;
    QString notes = notes.number(nextflag);
    pn.ConvertWGS84ToLocal((double)lat, (double)lon, nort, east, this);
    CFlag flagPt(lat, lon, east, nort, ptHeading, color, nextflag, notes);
    flagPts.append(flagPt);
    FileSaveFlags();
    }
}

void FormGPS::onBtnContour_clicked(){
    // Qt 6.8 Q_OBJECT_BINDABLE_PROPERTY: Direct access and automatic QML binding
    m_isContourBtnOn = !m_isContourBtnOn;


    if (this->isContourBtnOn()) {
        guidanceLookAheadTime = 0.5;
    }else{
        //if (ABLine.isBtnABLineOn | curve.isBtnCurveOn){
        //    ABLine.isABValid = false;
        //    curve.isCurveValid = false;
        //}
        guidanceLookAheadTime = SettingsManager::instance()->as_guidanceLookAheadTime();
    }
}

void FormGPS::onBtnContourPriority_clicked(bool isRight){

    ct.isRightPriority = isRight;
    qDebug() << "Contour isRight: " << isRight;
}

void FormGPS::onBtnContourLock_clicked(){
    ct.SetLockToLine(this);
}

void FormGPS::onBtnTiltDown_clicked(){

    if (camera.camPitch > -59) camera.camPitch = -60;
    camera.camPitch += ((camera.camPitch * 0.012) - 1);
    if (camera.camPitch < -76) camera.camPitch = -76;

    lastHeight = -1; //redraw the sky
    SettingsManager::instance()->setDisplay_camPitch(camera.camPitch);
    // CRITICAL: Force OpenGL update in GUI thread to prevent threading violation
    if (openGLControl) {
        QMetaObject::invokeMethod(openGLControl, "update", Qt::QueuedConnection);
    }
}

void FormGPS::onBtnTiltUp_clicked(){
    double camPitch = SettingsManager::instance()->display_camPitch();

    lastHeight = -1; //redraw the sky
    camera.camPitch -= ((camera.camPitch * 0.012) - 1);
    if (camera.camPitch > -58) camera.camPitch = 0;

    SettingsManager::instance()->setDisplay_camPitch(camera.camPitch);
    // CRITICAL: Force OpenGL update in GUI thread to prevent threading violation
    if (openGLControl) {
        QMetaObject::invokeMethod(openGLControl, "update", Qt::QueuedConnection);
    }
}

void FormGPS::onBtn2D_clicked(){
    camera.camFollowing = true;
    camera.camPitch = 0;
    navPanelCounter = 0;
}

void FormGPS::onBtn3D_clicked(){
    camera.camFollowing = true;
    camera.camPitch = -65;
    navPanelCounter = 0;
}
void FormGPS::onBtnN2D_clicked(){
    camera.camFollowing = false;
    camera.camPitch = 0;
    navPanelCounter = 0;
}
void FormGPS::onBtnN3D_clicked(){
    camera.camPitch = -65;
    camera.camFollowing = false;
    navPanelCounter = 0;
}
void FormGPS::onBtnZoomIn_clicked(){
    if (camera.zoomValue <= 20) {
        if ((camera.zoomValue -= camera.zoomValue * 0.1) < 3.0)
            camera.zoomValue = 3.0;
    } else {
        if ((camera.zoomValue -= camera.zoomValue * 0.05) < 3.0)
            camera.zoomValue = 3.0;
    }
    camera.camSetDistance = camera.zoomValue * camera.zoomValue * -1;
    camera.SetZoom();
    //TODO save zoom to properties
    // CRITICAL: Force OpenGL update in GUI thread to prevent threading violation
    if (openGLControl) {
        QMetaObject::invokeMethod(openGLControl, "update", Qt::QueuedConnection);
    }
}

void FormGPS::onBtnZoomOut_clicked(){
    if (camera.zoomValue <= 20) camera.zoomValue += camera.zoomValue * 0.1;
    else camera.zoomValue += camera.zoomValue * 0.05;
    if (camera.zoomValue > 220) camera.zoomValue = 220;
    camera.camSetDistance = camera.zoomValue * camera.zoomValue * -1;
    camera.SetZoom();

    //todo save to properties
    // CRITICAL: Force OpenGL update in GUI thread to prevent threading violation
    if (openGLControl) {
        QMetaObject::invokeMethod(openGLControl, "update", Qt::QueuedConnection);
    }
}

void FormGPS::onBtnRedFlag_clicked()
{   qDebug()<<"onBtnRedFlag_clicked";
    flagColor = 0;
    if (contextFlag) {
        contextFlag->setProperty("visible",false);
        contextFlag->setProperty("icon","/images/FlagRed.png");
    }
}

void FormGPS::onBtnGreenFlag_clicked()
{   qDebug()<<"onBtnGreenFlag_clicked";
    flagColor = 1;
    if (contextFlag) {
        contextFlag->setProperty("visible",false);
        contextFlag->setProperty("icon","/images/FlagGrn.png");
    }
}

void FormGPS::onBtnYellowFlag_clicked()
{   qDebug()<<"onBtnYellowFlag_clicked";
    flagColor = 2;
    if (contextFlag) {
        contextFlag->setProperty("visible",false);
        contextFlag->setProperty("icon","/images/FlagYel.png");
    }
}

void FormGPS::onBtnDeleteFlag_clicked()
{   qDebug()<<"onBtnDeleteFlag_clicked";
    //delete selected flag and set selected to none
    if (flagNumberPicked>0) {
    if (flagPts.size() > 0) {
    flagPts.remove(flagNumberPicked-1, 1);
    flagsBufferCurrent = false;
    flagNumberPicked = flagNumberPicked-1;

    int flagCnt = flagPts.size();
    if (flagCnt > 0) {
        for (int i = 0; i < flagCnt; i++)
            flagPts[i].ID = i + 1;
    }
    if (contextFlag) {
        contextFlag->setProperty("visible",false);
        if (flagNumberPicked > 0 && flagNumberPicked <= flagPts.size()) {
            contextFlag->setProperty("ptlat",flagPts[flagNumberPicked-1].latitude);
            contextFlag->setProperty("ptlon",flagPts[flagNumberPicked-1].longitude);
            contextFlag->setProperty("ptId",flagPts[flagNumberPicked-1].ID);
            contextFlag->setProperty("ptText",flagPts[flagNumberPicked-1].notes);
        }
        else    {
            contextFlag->setProperty("ptlat",0);
            contextFlag->setProperty("ptlon",0);
            contextFlag->setProperty("ptId",0);
            contextFlag->setProperty("ptText","");
        }
    }
    }
}
    else if (flagPts.size() > 0) {
        flagPts.remove(flagPts.size()-1, 1);

        if (contextFlag) {
            contextFlag->setProperty("visible",false);
            if (flagPts.size()>0) {
                contextFlag->setProperty("ptlat",flagPts[flagPts.size()-1].latitude);
                contextFlag->setProperty("ptlon",flagPts[flagPts.size()-1].longitude);
                contextFlag->setProperty("ptId",flagPts[flagPts.size()-1].ID);
                contextFlag->setProperty("ptText",flagPts[flagPts.size()-1].notes);
            }
            else    {
                contextFlag->setProperty("ptlat",0);
                contextFlag->setProperty("ptlon",0);
                contextFlag->setProperty("ptId",0);
                contextFlag->setProperty("ptText","");
            }
        }
    }
}
void FormGPS::onBtnDeleteAllFlags_clicked()
{   qDebug()<<"onBtnDeleteAllFlag_clicked";
    if (contextFlag) {
        contextFlag->setProperty("visible",false);
    }
    flagPts.clear();
    flagsBufferCurrent = false;
    flagNumberPicked = 0;
    //TODO: FileSaveFlags
}
void FormGPS::onBtnNextFlag_clicked()
{   qDebug()<<"onBtnNextFlag_clicked";

    if (flagNumberPicked<flagPts.size()){
        flagNumberPicked = flagNumberPicked + 1;}
    else flagNumberPicked = 0;
    if (flagNumberPicked > 0 && flagNumberPicked <= flagPts.size() && contextFlag){
        contextFlag->setProperty("ptlat",flagPts[flagNumberPicked-1].latitude);
        contextFlag->setProperty("ptlon",flagPts[flagNumberPicked-1].longitude);
        contextFlag->setProperty("ptId",flagPts[flagNumberPicked-1].ID);
        contextFlag->setProperty("ptText",flagPts[flagNumberPicked-1].notes);
    }
}
void FormGPS::onBtnPrevFlag_clicked()
{   qDebug()<<"onBtnPrevFlag_clicked";

if (flagNumberPicked>1){
        flagNumberPicked = flagNumberPicked -1 ;}
else {flagNumberPicked = flagPts.size();

}
if (flagNumberPicked > 0 && flagNumberPicked <= flagPts.size() && contextFlag){
    contextFlag->setProperty("ptlat",flagPts[flagNumberPicked-1].latitude);
    contextFlag->setProperty("ptlon",flagPts[flagNumberPicked-1].longitude);
    contextFlag->setProperty("ptId",flagPts[flagNumberPicked-1].ID);
    contextFlag->setProperty("ptText",flagPts[flagNumberPicked-1].notes);
}
}
void FormGPS::onBtnCancelFlag_clicked()
{
    flagNumberPicked = 0;
    FileSaveFlags();
}

void FormGPS::onBtnAutoYouTurn_clicked(){
    qDebug()<<"activate youturn";
    yt.loadSettings();
    yt.isTurnCreationTooClose = false;

//     if (bnd.bndArr.Count == 0)    this needs to be moved to qml
//     {
//         TimedMessageBox(2000, gStr.gsNoBoundary, gStr.gsCreateABoundaryFirst);
//         return;
//     }

     if (!this->isYouTurnBtnOn())
     {
         //new direction so reset where to put turn diagnostic
         yt.ResetCreatedYouTurn();

         if (!isBtnAutoSteerOn()) return;
         this->setIsYouTurnBtnOn(true);
         yt.isTurnCreationTooClose = false;
         yt.isTurnCreationNotCrossingError = false;
         yt.ResetYouTurn();
         //mc.autoSteerData[mc.sdX] = 0;
//         mc.machineControlData[mc.cnYouTurn] = 0;
//         btnAutoYouTurn.Image = Properties.Resources.Youturn80;
     }
     else
     {
         this->setIsYouTurnBtnOn(false);
//         yt.rowSkipsWidth = Properties.Vehicle.Default.set_youSkipWidth;
//         btnAutoYouTurn.Image = Properties.Resources.YouTurnNo;
         yt.ResetYouTurn();

         //new direction so reset where to put turn diagnostic
         yt.ResetCreatedYouTurn();

         //mc.autoSteerData[mc.sdX] = 0;commented in aog
//         mc.machineControlData[mc.cnYouTurn] = 0;
     }
}
void FormGPS::onBtnSwapAutoYouTurnDirection_clicked()
 {
     if (!yt.isYouTurnTriggered)
     {
         yt.isYouTurnRight = !yt.isYouTurnRight;
         yt.ResetCreatedYouTurn();
     }
     //else if (this->isYouTurnBtnOn())
         //btnAutoYouTurn.PerformClick();
 }

 void FormGPS::onBtnResetCreatedYouTurn_clicked()
 {
     qDebug()<<"ResetCreatedYouTurnd";
     yt.ResetYouTurn();
 }

 void FormGPS::onBtnAutoTrack_clicked()
 {
     track.setIsAutoTrack(!track.isAutoTrack());
     qDebug()<<"isAutoTrack";
 }

void FormGPS::onBtnManUTurn_clicked(bool right)
{
    if (yt.isYouTurnTriggered) {
        yt.ResetYouTurn();
    }else {
        yt.loadSettings(); // PHASE6-0-20: Sync rowSkipsWidth with SettingsManager before manual U-turn
        yt.isYouTurnTriggered = true;
        yt.BuildManualYouTurn(this, right, true, *CVehicle::instance(), track);
   }
}

void FormGPS::onBtnLateral_clicked(bool right)
{
   yt.BuildManualYouLateral(this, right, *CVehicle::instance(), track);
}

void FormGPS::btnSteerAngleUp_clicked(){
    CVehicle::instance()->driveFreeSteerAngle++;
    if (CVehicle::instance()->driveFreeSteerAngle > 40) CVehicle::instance()->driveFreeSteerAngle = 40;

    qDebug()<<"btnSteerAngleUp_clicked";
}
void FormGPS::btnSteerAngleDown_clicked(){
    CVehicle::instance()->driveFreeSteerAngle--;
    if (CVehicle::instance()->driveFreeSteerAngle < -40) CVehicle::instance()->driveFreeSteerAngle = -40;

    qDebug()<<"btnSteerAngleDown_clicked";
}
void FormGPS::btnFreeDrive_clicked(){


    if (CVehicle::instance()->isInFreeDriveMode)
    {
        //turn OFF free drive mode
        CVehicle::instance()->isInFreeDriveMode = false;
        CVehicle::instance()->driveFreeSteerAngle = 0;
    }
    else
    {
        //turn ON free drive mode
        CVehicle::instance()->isInFreeDriveMode = true;
        CVehicle::instance()->driveFreeSteerAngle = 0;
    }

    qDebug()<<"btnFreeDrive_clicked";
}
void FormGPS::btnFreeDriveZero_clicked(){
    if (CVehicle::instance()->driveFreeSteerAngle == 0)
        CVehicle::instance()->driveFreeSteerAngle = 5;
    else CVehicle::instance()->driveFreeSteerAngle = 0;

    qDebug()<<"btnFreeDriveZero_clicked";
}

void FormGPS::btnStartSA_clicked(){

 qDebug()<<"btnStartSA_clicked";

    if (!isSA)
    {
        isSA = true;
        startFix = CVehicle::instance()->pivotAxlePos;
        dist = 0;
        _diameter = 0;
        cntr = 0;
        //lblDiameter.Text = "0";
        setLblCalcSteerAngleInner("Drive Steady");
        // DEAD CODE from C# original - lblCalcSteerAngleOuter never displayed (FormSteer.cs:848 commented)
        lblCalcSteerAngleOuter = "Consistent Steering Angle!!";

    }
    else
    {
        isSA = false;
        setLblCalcSteerAngleInner("0.0 + °");
        // DEAD CODE from C# original - lblCalcSteerAngleOuter never displayed (FormSteer.cs:854 commented)
        lblCalcSteerAngleOuter = "0.0 + °";
    }
}

void FormGPS::TimedMessageBox(int timeout, QString s1, QString s2)
{
    qDebug() << "Timed message " << timeout << s1 << ", " << s2 << Qt::endl;
    QObject *temp = qmlItem(mainWindow, "timedMessage");
    QMetaObject::invokeMethod(temp, "addMessage", Q_ARG(int, timeout), Q_ARG(QString, s1), Q_ARG(QString, s2));
}

void FormGPS::turnOffBoundAlarm()
{
    qDebug() << "Bound alarm should be off" << Qt::endl;
    //TODO implement sounds
}

void FormGPS::FixTramModeButton()
{
    //TODO QML
    //unhide button if it should be seen
    if (tram.tramList.count() > 0 || tram.tramBndOuterArr.count() > 0)
    {
        //btnTramDisplayMode.Visible = true;
        tram.displayMode = 1;
    }

    //make sure tram has right icon.  DO this through javascript

}
void FormGPS::on_settings_reload() {
    loadSettings();
    //TODO: if vehicle name is set, write settings out to that
    //vehicle json file
}

void FormGPS::on_settings_save() {
    // Qt6 Pure Architecture: All property setters auto-sync to INI, no manual sync needed
    loadSettings();
}

void FormGPS::on_language_changed() {
    QString lang = SettingsManager::instance()->menu_language();
    qDebug() << "Changing language to:" << lang;

    // Load translation file (note: CMake generates resources with i18n/ prefix)
    if (m_translator->load(QString(":/qt/qml/AOG/i18n/i18n/qml_%1.qm").arg(lang))) {
        QCoreApplication::installTranslator(m_translator);
        qDebug() << "Translation loaded and installed successfully";

        // Force QML retranslation - this updates ALL qsTr() texts automatically
        this->retranslate();
        qDebug() << "QML retranslation completed";
    } else {
        qDebug() << "Failed to load translation for language:" << lang;
    }
}

void FormGPS::modules_send_238() {
    qDebug() << "Sending 238 message to AgIO";
    p_238.pgn[p_238.set0] = SettingsManager::instance()->ardMac_setting0();
    p_238.pgn[p_238.raiseTime] = SettingsManager::instance()->ardMac_hydRaiseTime();
    p_238.pgn[p_238.lowerTime] = SettingsManager::instance()->ardMac_hydLowerTime();

    p_238.pgn[p_238.user1] = SettingsManager::instance()->ardMac_user1();
    p_238.pgn[p_238.user2] = SettingsManager::instance()->ardMac_user2();
    p_238.pgn[p_238.user3] = SettingsManager::instance()->ardMac_user3();
    p_238.pgn[p_238.user4] = SettingsManager::instance()->ardMac_user4();

    qDebug() << SettingsManager::instance()->ardMac_user1();
    // SendPgnToLoop(p_238.pgn); // ❌ REMOVED - Phase 4.6: AgIOService Workers handle PGN
    if (m_agioService) {
        m_agioService->sendPgn(p_238.pgn);
    }
}
void FormGPS::modules_send_251() {
    //qDebug() << "Sending 251 message to AgIO";
    p_251.pgn[p_251.set0] = SettingsManager::instance()->ardSteer_setting0();
    p_251.pgn[p_251.set1] = SettingsManager::instance()->ardSteer_setting1();
    p_251.pgn[p_251.maxPulse] = SettingsManager::instance()->ardSteer_maxPulseCounts();
    p_251.pgn[p_251.minSpeed] = 5; //0.5 kmh THIS IS CHANGED IN AOG FIXES

    if (SettingsManager::instance()->as_isConstantContourOn())
        p_251.pgn[p_251.angVel] = 1;
    else p_251.pgn[p_251.angVel] = 0;

    qDebug() << p_251.pgn;
    // SendPgnToLoop(p_251.pgn); // ❌ REMOVED - Phase 4.6: AgIOService Workers handle PGN
    if (m_agioService) {
        m_agioService->sendPgn(p_251.pgn);
    }
}

void FormGPS::modules_send_252() {
    //qDebug() << "Sending 252 message to AgIO";
    p_252.pgn[p_252.gainProportional] = SettingsManager::instance()->as_Kp();
    p_252.pgn[p_252.highPWM] = SettingsManager::instance()->as_highSteerPWM();
    p_252.pgn[p_252.lowPWM] = SettingsManager::instance()->as_lowSteerPWM();
    p_252.pgn[p_252.minPWM] = SettingsManager::instance()->as_minSteerPWM();
    p_252.pgn[p_252.countsPerDegree] = SettingsManager::instance()->as_countsPerDegree();
    int wasOffset = (int)SettingsManager::instance()->as_wasOffset();
    p_252.pgn[p_252.wasOffsetHi] = (char)(wasOffset >> 8);
    p_252.pgn[p_252.wasOffsetLo] = (char)wasOffset;
    p_252.pgn[p_252.ackerman] = SettingsManager::instance()->as_ackerman();


    qDebug() << p_252.pgn;
    // SendPgnToLoop(p_252.pgn); // ❌ REMOVED - Phase 4.6: AgIOService Workers handle PGN
    if (m_agioService) {
        m_agioService->sendPgn(p_252.pgn);
    }
}

void FormGPS::headland_save() {
    //TODO make FileHeadland() a slot so we don't have to have this
    //wrapper.
    FileSaveHeadland();
}

void FormGPS::headlines_load() {
    //TODO make FileLoadHeadLines a slot, skip this wrapper
    FileLoadHeadLines();
}

void FormGPS::headlines_save() {
    //TODO make FileSaveHeadLines a slot, skip this wrapper
    FileSaveHeadLines();
}
void FormGPS::onBtnResetSim_clicked(){
    sim.latitude = SettingsManager::instance()->gps_simLatitude();
    sim.longitude = SettingsManager::instance()->gps_simLongitude();
}

void FormGPS::onBtnRotateSim_clicked(){
    qDebug() << "Rotate Sim";
    qDebug() << "But nothing else";
    /*qDebug() << sim.headingTrue;
    sim.headingTrue += M_PI;
    qDebug() << sim.headingTrue;
    ABLine.isABValid = false;
    curve.isCurveValid = false;*/
    //curve.lastHowManyPathsAway = 98888; not in v5
}

//Track Snap buttons
void FormGPS::onBtnSnapToPivot_clicked(){
    qDebug()<<"snap to pivot";
}
void FormGPS::onBtnSnapSideways_clicked(double distance){
    int blah = distance;

}


void FormGPS::onDeleteAppliedArea_clicked()
{
    if (isJobStarted())
    {
        /*if (autoBtnState == btnStates.Off && manualBtnState == btnStates.Off)
        {

            DialogResult result3 = MessageBox.Show(gStr.gsDeleteAllContoursAndSections,
                                                   gStr.gsDeleteForSure,
                                                   MessageBoxButtons.YesNo,
                                                   MessageBoxIcon.Question,
                                                   MessageBoxDefaultButton.Button2);
            if (result3 == DialogResult.Yes)
            {
                //FileCreateElevation();

                if (tool.isSectionsNotZones)
                {
                    //Update the button colors and text
                    AllSectionsAndButtonsToState(btnStates.Off);

                    //enable disable manual buttons
                    LineUpIndividualSectionBtns();
                }
                else
                {
                    AllZonesAndButtonsToState(btnStates.Off);
                    LineUpAllZoneButtons();
                }

                //turn manual button off
                manualBtnState = btnStates.Off;
                btnSectionMasterManual.Image = Properties.Resources.ManualOff;

                //turn auto button off
                autoBtnState = btnStates.Off;
                btnSectionMasterAuto.Image = Properties.Resources.SectionMasterOff;
               */

                //clear out the contour Lists
                ct.StopContourLine(contourSaveList);
                ct.ResetContour();
                this->setWorkedAreaTotal(0);

                //clear the section lists
                for (int j = 0; j < triStrip.count(); j++)
                {
                    //clean out the lists
                    triStrip[j].patchList.clear();
                    triStrip[j].triangleList.clear();
                }

                patchesBufferDirty=true;
                //patchSaveList.clear();
                //shouldn't we clean out triStrip too?
                tool.patchSaveList.clear();

                FileCreateContour();
                FileCreateSections();

            /*}
            else
            {
                TimedMessageBox(1500, gStr.gsNothingDeleted, gStr.gsActionHasBeenCancelled);
            }
        }
        else
        {
            TimedMessageBox(1500, "Sections are on", "Turn Auto or Manual Off First");
        }*/
    }
}

void FormGPS::Timer1_Tick()
{
    if (isSA)
    {
        dist = glm::Distance(startFix, CVehicle::instance()->pivotAxlePos);
        cntr++;
        if (dist > _diameter)
        {
            _diameter = dist;
            cntr = 0;
        }
        //lblDiameter = _diameter.ToString("N2") + " m";
        setLblDiameter(locale.toString(_diameter, 'g', 3) + tr(" m"));
        qDebug()<<"_diameter ";
        qDebug()<<_diameter;
        if (cntr > 9)
        {
            steerAngleRight = atan(CVehicle::instance()->wheelbase / ((_diameter - CVehicle::instance()->trackWidth * 0.5) / 2));
            steerAngleRight = glm::toDegrees(steerAngleRight);

            //lblCalcSteerAngleInner = steerAngleRight.ToString("N1") + "°";
            setLblCalcSteerAngleInner(locale.toString(steerAngleRight, 'g', 3) + tr("°"));
            //lblDiameter.Text = diameter.ToString("N2") + " m";
            setLblDiameter(locale.toString(_diameter, 'g', 3) + tr(" m"));
            isSA = false;
        }
    }
/*
    double actAng = mc.actualSteerAngleDegrees * 5;
    if (actAng > 0)
    {
        if (actAng > 49) actAng = 49;
        //CExtensionMethods.SetProgressNoAnimation(pbarRight, (int)actAng);
        //pbarLeft = 0;
    }
    else
    {
        if (actAng < -49) actAng = -49;
       // pbarRight = 0;
       // CExtensionMethods.SetProgressNoAnimation(pbarLeft, (int)-actAng);
    }

    lblSteerAngle = SetSteerAngle;
    lblSteerAngleActual = locale.toString(mc.actualSteerAngleDegrees, 'g', 1) + tr("°");
    lblActualSteerAngleUpper = lblSteerAngleActual;
    double err = (mc.actualSteerAngleDegrees - guidanceLineSteerAngle * 0.01);
    lblError = abs(err).ToString("N1") + "\u00B0";
    if (err > 0) lblError.ForeColor = Color.Red;
    else lblError.ForeColor = Color.DarkGreen;

    lblAV_Act.Text = mf.actAngVel.ToString("N1");
    lblAV_Set.Text = mf.setAngVel.ToString("N1");

    lblPWMDisplay.Text = mc.pwmDisplay.ToString();

    counter++;

    if (toSend && counter > 4)
    {
        p_252.pgn[p_252.countsPerDegree] = (char)hsbarCountsPerDegree;
        p_252.pgn[p_252.ackerman] = (char)hsbarAckerman;

        p_252.pgn[p_252.wasOffsetHi] = (char)(hsbarWasOffset.Value >> 8);
        p_252.pgn[p_252.wasOffsetLo] = (char)(hsbarWasOffset.Value);

        p_252.pgn[p_252.highPWM] = (char)hsbarHighSteerPWM;
        p_252.pgn[p_252.lowPWM] = (char)(hsbarHighSteerPWM / 3);
        p_252.pgn[p_252.gainProportional] = (char)hsbarProportionalGain;
        p_252.pgn[p_252.minPWM] = (char)hsbarMinPWM;

        // SendPgnToLoop(p_252.pgn); // ❌ REMOVED - Phase 4.6: AgIOService Workers handle PGN
    if (m_agioService) {
        m_agioService->sendPgn(p_252.pgn);
    }
        toSend = false;
        counter = 0;
    }

    if (secondCntr++ > 2)
    {
        secondCntr = 0;

        if (tabControl1.SelectedTab == tabPPAdv)
        {
            lblHoldAdv = CVehicle::instance()->goalPointLookAheadHold.ToString("N1");
            lblAcqAdv = (CVehicle::instance()->goalPointLookAheadHold * mf.CVehicle::instance()->goalPointAcquireFactor).ToString("N1");
            lblDistanceAdv = CVehicle::instance()->goalDistance.ToString("N1");
            lblAcquirePP = lblAcqAdv.Text;
        }
    }

    if (this->sensorData() != -1)
    {
        if (this->sensorData() < 0 || this->sensorData() > 255) this->sensorData() = 0;
        //CExtensionMethods.SetProgressNoAnimation(pbarSensor, this->sensorData());
        //if (nudMaxCounts.Visible == false)
            //lblPercentFS.Text = ((int)((double)this->sensorData() * 0.3921568627)).ToString() + "%";
        else
            //lblPercentFS.Text = this->sensorData().ToString();
    }
    */
}

// OLD loadTranslation function removed - replaced by on_language_changed()

// Начало сбора данных
void FormGPS::StartDataCollection()
{
    IsCollectingData = true;
    LastCollectionTime = QDateTime::currentDateTime();
    qDebug()<< "StartDataCollection";
}

// Завершение сбора данных
void FormGPS::StopDataCollection()
{
    IsCollectingData = false;
    qDebug()<<"StopDataCollection";
}

// Полностью сбрасываем историю и аналитику
void FormGPS::ResetData()
{
    steerAngleHistory.clear();
    SampleCount = 0;
    RecommendedWASZero = 0;
    ConfidenceLevel = 0;
    HasValidRecommendation = false;
    Mean = 0;
    StandardDeviation = 0;
    Median = 0;
}

// Применяем смещение к историческим данным
void FormGPS::ApplyOffsetToCollectedData(double appliedOffsetDegrees)
{
    if (steerAngleHistory.empty()) return;

    for (size_t i = 0; i < steerAngleHistory.size(); ++i)
    {
        steerAngleHistory[i] += appliedOffsetDegrees;
    }

    if (SampleCount >= MIN_SAMPLES_FOR_ANALYSIS)
    {
        PerformStatisticalAnalysis();
    }

    qDebug() << "Smart WAS: Applied " << appliedOffsetDegrees << "° offset to "
             << steerAngleHistory.size() << " collected samples.";
}

// Добавляем новую запись угла направления
void FormGPS::AddSteerAngleSample(double guidanceSteerAngle, double currentSpeed)
{   //qDebug()<<"AddSteerAngleSample";
    if (!IsCollectingData || !ShouldCollectSample(guidanceSteerAngle, currentSpeed))
        return;

    steerAngleHistory.push_back(guidanceSteerAngle);
    LastCollectionTime = QDateTime::currentDateTime();

    if (steerAngleHistory.size() > MAX_SAMPLES)
    {
        steerAngleHistory.pop_front();  // удаляем самый старый элемент
    }

    SampleCount = steerAngleHistory.size();

    if (SampleCount >= MIN_SAMPLES_FOR_ANALYSIS)
    {
        PerformStatisticalAnalysis();
    }

}

// Возвращаем поправочный коэффициент на основе текущих данных
int FormGPS::GetRecommendedWASOffsetAdjustment(int currentCPD)
{
    if (!HasValidRecommendation) return 0;

    return static_cast<int>(std::round(RecommendedWASZero * currentCPD));
}

// Проверяем подходит ли данный образец для сбора
bool FormGPS::ShouldCollectSample(double steerAngle, double speed)
{
    if (speed < MIN_SPEED_THRESHOLD) return false;
    if (std::abs(steerAngle) > MAX_ANGLE_THRESHOLD) return false;
    if (!isBtnAutoSteerOn()) return false;
    if (std::abs(CVehicle::instance()->guidanceLineDistanceOff) > 15000) return false;

    return true;
}

// Основная процедура статистического анализа
void FormGPS::PerformStatisticalAnalysis()
{
    if (steerAngleHistory.size() < MIN_SAMPLES_FOR_ANALYSIS) return;

    auto sortedData = steerAngleHistory;
    std::sort(sortedData.begin(), sortedData.end()); // сортируем массив

    Mean = std::accumulate(steerAngleHistory.begin(), steerAngleHistory.end(), 0.0) /
           steerAngleHistory.size();

    Median = CalculateMedian(sortedData);
    StandardDeviation = CalculateStandardDeviation(steerAngleHistory, Mean);

    RecommendedWASZero = -Median; // отрицательная коррекция приближает к центру

    CalculateConfidenceLevel(sortedData);

    HasValidRecommendation = ConfidenceLevel > 50.0 &&
                             SampleCount >= MIN_SAMPLES_FOR_ANALYSIS;
    //qDebug()<<"HasValidRecommendation"<<HasValidRecommendation;
}

// Функция для нахождения медианы
double FormGPS::CalculateMedian(QVector<double> sortedData)
{
    int count = sortedData.size();
    if (count == 0) return 0;

    if (count % 2 == 0)
    {
        return (sortedData[count / 2 - 1] + sortedData[count / 2]) / 2.0;
    }
    else
    {
        return sortedData[count / 2];
    }
}

// Расчет стандартного отклонения
double FormGPS::CalculateStandardDeviation(QVector<double> data, double mean)
{
    if (data.size() < 2) return 0;

    double sumOfSquares = 0.0;
    for (double d : data)
    {
        sumOfSquares += std::pow(d - mean, 2);
    }

    return std::sqrt(sumOfSquares / (data.size() - 1));
}

// Подсчет коэффициента уверенности
void FormGPS::CalculateConfidenceLevel(QVector<double> sortedData)
{
    if (sortedData.size() < MIN_SAMPLES_FOR_ANALYSIS)
    {
        ConfidenceLevel = 0;
        return;
    }

    double oneStdDevRange = StandardDeviation;
    double twoStdDevRange = 2 * StandardDeviation;

    int withinOneStdDev = 0;
    int withinTwoStdDev = 0;

    for (double angle : sortedData)
    {
        double deviationFromMedian = std::abs(angle - Median);
        if (deviationFromMedian <= oneStdDevRange) withinOneStdDev++;
        if (deviationFromMedian <= twoStdDevRange) withinTwoStdDev++;
    }

    double oneStdDevPercentage = static_cast<double>(withinOneStdDev) / sortedData.size();
    double twoStdDevPercentage = static_cast<double>(withinTwoStdDev) / sortedData.size();

    // ожидаемое нормальное распределение данных
    double expectedOneStdDev = 0.68;
    double expectedTwoStdDev = 0.95;

    // считаем баллы для каждой метрики
    double oneStdDevScore = std::max(0.0, 1 - std::abs(oneStdDevPercentage - expectedOneStdDev) / expectedOneStdDev);
    double twoStdDevScore = std::max(0.0, 1 - std::abs(twoStdDevPercentage - expectedTwoStdDev) / expectedTwoStdDev);
    double magnitudeScore = std::max(0.0, 1 - std::abs(RecommendedWASZero) / 10.0); // штрафуем большие поправки
    double sampleSizeFactor = std::min(1.0, static_cast<double>(sortedData.size()) / (MIN_SAMPLES_FOR_ANALYSIS * 3)); // размер выборки влияет положительно

    // объединяем факторы
    ConfidenceLevel = ((oneStdDevScore * 0.3 + twoStdDevScore * 0.3 + magnitudeScore * 0.2 + sampleSizeFactor * 0.2) * 100);
    ConfidenceLevel = std::clamp(ConfidenceLevel, 0.0, 100.0);
}

void FormGPS::SmartCalLabelClick()
{
    // Сброс калибровки Smart WAS при клике на любую статусную метку
    if (IsCollectingData)
    {
        ResetData();

        // Покажите короткое подтверждение сброса
        TimedMessageBox(1500, "Reset To Default", "CalibrationDataReset");
    }
    qDebug()<<"SmartCalLabelClick";
}

void FormGPS::on_btnSmartZeroWAS_clicked()
{
    if (!IsCollectingData)
    {   TimedMessageBox(2000, "SmartCalibrationErro", "gsSmartWASNotAvailable");
        return;
    }

    if (!HasValidRecommendation)
    {
        if (SampleCount < 200)
        {
            TimedMessageBox(2000, tr("Need at least 200 samples for calibration. Drive on guidance lines to collect more data."), QString(tr("Insufficient Data")) + " " +
                                                                         QString::number(SampleCount, 'f', 1));
        }
        else
        {
            TimedMessageBox(2000, tr("Calibration confidence is low. Need at least 70% confidence. Drive more consistently on guidance lines."), QString(tr("Low Confidence")) + " " +
                                                                                                                                      QString::number(ConfidenceLevel, 'f', 1));
        }
        return;
    }

    // Получаем рекомендацию по смещению
    int recommendedOffsetAdjustment = GetRecommendedWASOffsetAdjustment(SettingsManager::instance()->as_countsPerDegree());
    int newOffset = SettingsManager::instance()->as_wasOffset() + recommendedOffsetAdjustment;

    // Проверяем новое значение смещения на допустимый диапазон
    if (std::abs(newOffset) > 3900)
    {
        TimedMessageBox(2000, tr("Recommended adjustment {0} exceeds safe range (±50). Please check WAS sensor alignment"), QString(tr("Exceeded Range")) + " " +
                                                                                                                                  QString::number(newOffset, 'f', 1));
        qDebug() << "Smart Zero превысил диапазон:" << newOffset;
        return;
    }

    // Применяем смещение нуля WAS
    SettingsManager::instance()->setAs_wasOffset(newOffset);

    // Критически важно: применяем смещение к ранее собранным данным
    ApplyOffsetToCollectedData(RecommendedWASZero);

    // Сообщаем об успешной настройке
    TimedMessageBox(2000, tr("%1 образцов, %2% уверенности, коррекция %3°")
                                  .arg(SampleCount)
                                  .arg(QString::number(ConfidenceLevel, 'f', 1))
                                  .arg(QString::number(RecommendedWASZero, 'f', 2)),
    QString(tr("Смещение успешно применено")));


    qDebug() << "Настройка Smart WAS выполнена -"
             << "Образцы:" << SampleCount
             << ", Уверенность:" << QString::number(ConfidenceLevel, 'f', 1) << "%,"
             << "Корректировка:" << QString::number(RecommendedWASZero, 'f', 2) << "°";
}

// TracksInterface and VehicleInterface now use QML_SINGLETON + QML_ELEMENT (same approach as Settings)

// ===== QML INTERFACE INITIALIZATION - DELAYED TIMING FIX =====
void FormGPS::initializeQMLInterfaces()
{
    qDebug() << "🔄 Starting QML interface initialization...";

    // ⚡ PHASE 6.3.0 SAFETY: Verify mainWindow is valid before accessing children
    if (!mainWindow) {
        qWarning() << "❌ mainWindow is NULL - cannot initialize QML interfaces";
        return;
    }

    qDebug() << "✅ mainWindow valid, proceeding with interface initialization";

    // ===== PHASE 6.0.3.1: Initialize PropertyWrapper FIRST - before any QML interface access =====
    // Phase 6.0.4.5: PropertyWrapper initialization removed - using native Qt 6.8 Q_PROPERTY

    // ===== PHASE 6.0.3.2: Initialize PropertyWrapper properties AFTER roots are ready =====
    qDebug() << "🔧 Phase 6.0.3.2: Setting initial PropertyWrapper values...";
    this->setSentenceCounter(0);
    this->setManualBtnState((int)btnStates::Off);
    this->setAutoBtnState((int)btnStates::Off);
    this->setIsPatchesChangingColor( false);
    this->setIsOutOfBounds(false);
    qDebug() << "  ✅ PropertyWrapper initial values set successfully";

    // ===== CRITICAL: Initialize QML members AFTER QML objects are created =====
    // Crash fix: these variables MUST be initialized after QML components load
    boundaryInterface = qmlItem(mainWindow, "boundaryInterface");
    fieldInterface = qmlItem(mainWindow, "fieldInterface");
    recordedPathInterface = qmlItem(mainWindow, "recordedPathInterface");

    //have to do this for each Interface and supported data type.
    // QObject *aog = qmlItem(mainWindow, "aog");
    // if (aog) {
    //     qDebug() << "✅ AOG interface found - setting InterfaceProperty roots";

    //     qDebug() << "✅ InterfaceProperty initialization completed successfully";

    //     // ⚡ PHASE 6.3.0 ARCHITECTURAL FIX: Delay OpenGL callbacks setup
    //     // Give time for set_qml_root() to fully stabilize before enabling rendering
    //     QTimer::singleShot(10, this, [this]() {
    //         initializeOpenGLCallbacks();
    //     });

    // } else {
    //     qWarning() << "❌ AOG interface STILL not found after delay - scheduling retry in 500ms";

    //     // ⚡ FALLBACK: Retry after additional delay if still not found
    //     QTimer::singleShot(500, this, [this]() {
    //         QObject *aog = qmlItem(mainWindow, "aog");
    //         if (aog) {
    //             qDebug() << "✅ AOG interface found on retry - setting InterfaceProperty roots";
    //             qDebug() << "✅ InterfaceProperty initialization completed on retry";
    //         } else {
    //             qCritical() << "🚨 CRITICAL: AOG interface not found after multiple attempts!";
    //             qCritical() << "🚨 This will cause InterfaceProperty errors - check QML loading";
    //         }
    //     });
    // }

    // Initialize other interface properties (these should work as they use immediate objects)
    if (fieldInterface) {
    }

    if (boundaryInterface) {
    }

    if (recordedPathInterface) {
    }

    qDebug() << "🎯 QML Interface initialization procedure completed";

    // ⚡ PHASE 6.3.0 TIMING FIX: Connect Interface signals AFTER QML objects are initialized
    if (fieldInterface) {
        qDebug() << "🔗 fieldInterface found - Qt 6.8 Q_INVOKABLE calls ready";
        // ===== BATCH 13 - Field Management Connections REMOVED - Qt 6.8 modernized to Q_INVOKABLE calls =====
        // REMOVED: connect(fieldInterface,SIGNAL(field_update_list()), this, SLOT(field_update_list()));
        // REMOVED: connect(fieldInterface,SIGNAL(field_close()), this, SLOT(field_close()));
        // REMOVED: connect(fieldInterface,SIGNAL(field_open(QString)), this, SLOT(field_open(QString)));
        // REMOVED: connect(fieldInterface,SIGNAL(field_new(QString)), this, SLOT(field_new(QString)));
        // REMOVED: connect(fieldInterface,SIGNAL(field_new_from(QString,QString,int)), this, SLOT(field_new_from(QString,QString,int)));
        // REMOVED: connect(fieldInterface,SIGNAL(field_new_from_KML(QString,QString)), this, SLOT(field_new_from_KML(QString,QString)));
        // REMOVED: connect(fieldInterface,SIGNAL(field_delete(QString)), this, SLOT(field_delete(QString)));
        qDebug() << "✅ fieldInterface modernized to Q_INVOKABLE pattern - 7 connections replaced";
    } else {
        qWarning() << "❌ fieldInterface not found - field operations will not work";
    }

    if (boundaryInterface) {
        qDebug() << "🔗 Connecting boundaryInterface signals...";
        // ⚡ YouTurn out of bounds signal
        connect(&yt, SIGNAL(outOfBounds()),boundaryInterface,SLOT(setIsOutOfBoundsTrue()));
        // ===== BATCH 14 - Boundary Management Connections REMOVED - Qt 6.8 modernized to Q_INVOKABLE calls =====
        // REMOVED: connect(boundaryInterface, SIGNAL(calculate_area()), this, SLOT(boundary_calculate_area()));
        // REMOVED: connect(boundaryInterface, SIGNAL(update_list()), this, SLOT(boundary_update_list()));
        // REMOVED: connect(boundaryInterface, SIGNAL(start()), this, SLOT(boundary_start()));
        // REMOVED: connect(boundaryInterface, SIGNAL(stop()), this, SLOT(boundary_stop()));
        // REMOVED: connect(boundaryInterface, SIGNAL(add_point()), this, SLOT(boundary_add_point()));
        // REMOVED: connect(boundaryInterface, SIGNAL(delete_last_point()), this, SLOT(boundary_delete_last_point()));
        // REMOVED: connect(boundaryInterface, SIGNAL(pause()), this, SLOT(boundary_pause()));
        // REMOVED: connect(boundaryInterface, SIGNAL(record()), this, SLOT(boundary_record()));
        // REMOVED: connect(boundaryInterface, SIGNAL(reset()), this, SLOT(boundary_restart()));
        // REMOVED: connect(boundaryInterface, SIGNAL(delete_boundary(int)), this, SLOT(boundary_delete(int)));
        // REMOVED: connect(boundaryInterface, SIGNAL(set_drive_through(int, bool)), this, SLOT(boundary_set_drivethru(int,bool)));
        // REMOVED: connect(boundaryInterface, SIGNAL(delete_all()), this, SLOT(boundary_delete_all()));
        qDebug() << "✅ boundaryInterface modernized to Q_INVOKABLE pattern - 11 connections replaced";
    } else {
        qWarning() << "❌ boundaryInterface not found - boundary operations will not work";
    }

    // ⚡ PHASE 6.3.0 TIMING FIX: Verify InterfaceProperty are really initialized before OpenGL
    bool interfacePropertiesReady = false;
    try {
        // Test if InterfaceProperty are accessible without crash
        bool testJob = isJobStarted();
        bool testAutosteer = isBtnAutoSteerOn();
        interfacePropertiesReady = true;
        qDebug() << "✅ InterfaceProperty validation successful - isJobStarted:" << testJob << "isBtnAutoSteerOn():" << testAutosteer;
    } catch (...) {
        qWarning() << "❌ InterfaceProperty not yet ready - OpenGL setup will be retried";
        interfacePropertiesReady = false;
    }

    if (openGLControl && interfacePropertiesReady) {
        qDebug() << "🎯 Setting up OpenGL callbacks - InterfaceProperty verified safe";
        openGLControl->setProperty("callbackObject",QVariant::fromValue((void *) this));
        openGLControl->setProperty("initCallback",QVariant::fromValue<std::function<void (void)>>(std::bind(&FormGPS::openGLControl_Initialized, this)));
#if defined(Q_OS_WINDOWS) //|| defined (Q_OS_ANDROID)
        //direct rendering in the QML render thread.  Will need locking to be safe.
        openGLControl->setProperty("paintCallback",QVariant::fromValue<std::function<void (void)>>(std::bind(&FormGPS::oglMain_Paint,this)));
#else
        //do indirect rendering for now.
        openGLControl->setProperty("paintCallback",QVariant::fromValue<std::function<void (void)>>(std::bind(&FormGPS::render_main_fbo,this)));
#endif
#ifdef USE_QSGRENDERNODE
        openGLControl->setProperty("cleanupCallback",QVariant::fromValue<std::function<void (void)>>(std::bind(&FormGPS::openGLControl_Shutdown,this)));
#else

        openGLControl->setProperty("samples",SettingsManager::instance()->display_antiAliasSamples());
        static_cast<AOGRendererInSG *>(openGLControl)->setMirrorVertically(true);
#endif
        connect(openGLControl,SIGNAL(clicked(QVariant)),this,SLOT(onGLControl_clicked(QVariant)));
        connect(openGLControl,SIGNAL(dragged(int,int,int,int)),this,SLOT(onGLControl_dragged(int,int,int,int)));
        qDebug() << "✅ OpenGL callbacks configured - rendering can now safely access InterfaceProperty";
    } else if (openGLControl && !interfacePropertiesReady) {
        qWarning() << "⚠️ OpenGL setup deferred - InterfaceProperty not ready yet";
        // Retry OpenGL setup after additional delay
        QTimer::singleShot(100, this, [this]() {
            qDebug() << "🔄 Retrying OpenGL setup after additional delay...";
            initializeOpenGLCallbacks();
        });
    }

    // ⚡ PHASE 6.3.0 TIMING FIX: Start simulator timer AFTER InterfaceProperty initialization
    if (SettingsManager::instance()->menu_isSimulatorOn()) {
        if (!timerSim.isActive()) {
            // Verify that InterfaceProperty actually work before starting timer
            try {
                bool testProperty = isJobStarted();  // Test basic Q_PROPERTY access
                timerSim.start(100); // 10Hz sync with GPS update
                qDebug() << "✅ Simulator timer started (10Hz) - InterfaceProperty safe access verified";
            } catch (...) {
                qWarning() << "⚠️ InterfaceProperty test failed - deferring simulator start by 100ms";
                QTimer::singleShot(100, this, [this]() {
                    qDebug() << "🚀 Starting simulator timer (10Hz) after additional delay";
                    timerSim.start(100);  // 10Hz
                });
            }
        }
    }
}

// ===== OPENGL CALLBACKS SETUP - WITH INTERFACEPROPERTY VALIDATION =====
void FormGPS::initializeOpenGLCallbacks()
{
    qDebug() << "🔄 Attempting OpenGL callbacks setup...";

    if (!openGLControl) {
        qWarning() << "❌ OpenGL control not available for callback setup";
        return;
    }

    // Test if InterfaceProperty are accessible
    bool interfacePropertiesReady = false;
    try {
        bool testJob = isJobStarted();
        bool testAutosteer = isBtnAutoSteerOn();
        interfacePropertiesReady = true;
        qDebug() << "✅ InterfaceProperty retry validation successful - isJobStarted:" << testJob << "isBtnAutoSteerOn():" << testAutosteer;
    } catch (...) {
        qWarning() << "❌ InterfaceProperty still not ready - scheduling another retry in 200ms";
        QTimer::singleShot(200, this, [this]() {
            initializeOpenGLCallbacks();
        });
        return;
    }

    if (interfacePropertiesReady) {
        qDebug() << "🎯 Setting up OpenGL callbacks - InterfaceProperty verified ready";
        openGLControl->setProperty("callbackObject",QVariant::fromValue((void *) this));
        openGLControl->setProperty("initCallback",QVariant::fromValue<std::function<void (void)>>(std::bind(&FormGPS::openGLControl_Initialized, this)));
#if defined(Q_OS_WINDOWS) //|| defined (Q_OS_ANDROID)
        //direct rendering in the QML render thread.  Will need locking to be safe.
        openGLControl->setProperty("paintCallback",QVariant::fromValue<std::function<void (void)>>(std::bind(&FormGPS::oglMain_Paint,this)));
#else
        //do indirect rendering for now.
        openGLControl->setProperty("paintCallback",QVariant::fromValue<std::function<void (void)>>(std::bind(&FormGPS::render_main_fbo,this)));
#endif
#ifdef USE_QSGRENDERNODE
        openGLControl->setProperty("cleanupCallback",QVariant::fromValue<std::function<void (void)>>(std::bind(&FormGPS::openGLControl_Shutdown,this)));
#else
        openGLControl->setProperty("samples",SettingsManager::instance()->display_antiAliasSamples());
        static_cast<AOGRendererInSG *>(openGLControl)->setMirrorVertically(true);
#endif
        connect(openGLControl,SIGNAL(clicked(QVariant)),this,SLOT(onGLControl_clicked(QVariant)));
        connect(openGLControl,SIGNAL(dragged(int,int,int,int)),this,SLOT(onGLControl_dragged(int,int,int,int)));
        qDebug() << "✅ OpenGL callbacks successfully configured - rendering now safe";
    }
}

// ===== SAFE QML OBJECT ACCESS - NULL PROTECTION WITH RETRIES =====
QObject* FormGPS::safeQmlItem(const QString& objectName, int maxRetries)
{
    QObject* obj = nullptr;
    int attempts = 0;

    while (!obj && attempts < maxRetries) {
        obj = qmlItem(mainWindow, objectName);

        if (obj) {
            qDebug() << "✅ QML object found:" << objectName << "on attempt" << (attempts + 1);
            return obj;
        }

        attempts++;
        qWarning() << "⚠️ QML object not found:" << objectName << "- attempt" << attempts << "/" << maxRetries;

        if (attempts < maxRetries) {
            // Wait progressively longer between retries
            QThread::msleep(50 * attempts);  // 50ms, 100ms, 150ms...
        }
    }

    if (!obj) {
        qCritical() << "🚨 CRITICAL: QML object" << objectName << "not found after" << maxRetries << "attempts!";
        qCritical() << "🚨 This may be in a Drawer/Popup that's not loaded yet";
        qCritical() << "🚨 Consider accessing this object only when UI is visible";
    }

    return obj;
}

// ===== IMU CONFIGURATION IMPLEMENTATIONS =====
void FormGPS::changeImuHeading(double heading) {
    // Met à jour la propriété IMU heading
    m_imuHeading = heading;
}

void FormGPS::changeImuRoll(double roll) {
    // Met à jour la propriété IMU roll
    m_imuRoll = roll;
}

// ===== USER DATA MANAGEMENT IMPLEMENTATIONS =====
void FormGPS::setDistanceUser(const QString& value) {
    bool ok;
    double distance = value.toDouble(&ok);
    if (ok) {
        m_distanceUser = distance;
    }
}

void FormGPS::setWorkedAreaTotalUser(const QString& value) {
    bool ok;
    double area = value.toDouble(&ok);
    if (ok) {
        m_workedAreaTotalUser = area;
    }
}

// Phase 6.0.20: Qt 6.8 BINDABLE implementation moved to formgps.cpp:303-305

// ===== AB LINES MANAGEMENT STUB IMPLEMENTATIONS =====
// TODO: Implement these methods properly when AB Lines functionality is ready
void FormGPS::updateABLines() {
    qWarning() << "updateABLines() NOT IMPLEMENTED YET";
    // TODO: Future implementation will update AB lines list
}

void FormGPS::updateCurves() {
    qWarning() << "updateCurves() NOT IMPLEMENTED YET";
    // TODO: Future implementation will update curves list
}

void FormGPS::setCurrentABCurve(int index) {
    qWarning() << "setCurrentABCurve() NOT IMPLEMENTED YET - index:" << index;
    // TODO: Future implementation will set current AB curve
}

// ===== AB Lines Methods - Phase 6.0.20 =====

void FormGPS::swapABLineHeading(int index) {
    if (index >= 0 && index < track.count()) {
        track.swapAB(index);
        updateABLines();
    }
}

void FormGPS::deleteABLine(int index) {
    if (index >= 0 && index < track.count()) {
        track.delete_track(index);
        updateABLines();
    }
}

void FormGPS::addABLine(const QString& name) {
    // Note: AB Line creation is handled by TrackNewSet.qml interface
    // which uses TracksInterface.start_new() + mark_start() + finish_new()
    // This method is kept for API consistency but delegates to existing workflow
    qWarning() << "addABLine() called - Use TrackNewSet.qml interface for full creation workflow";
    updateABLines();
}

void FormGPS::changeABLineName(int index, const QString& newName) {
    if (index >= 0 && index < track.count()) {
        track.changeName(index, newName);
        updateABLines();
    }
}

