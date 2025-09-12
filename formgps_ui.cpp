#include <QQuickView>
#include <QQuickWidget>
#include <QQmlContext>
#include <QScreen>
#include "formgps.h"
#include "qmlutil.h"
#include <QTimer>
#include "cvehicle.h"
#include "ccontour.h"
#include "cabline.h"
#include "newsettings.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <functional>
#include <assert.h>
#include "aogrenderer.h"
#include "qmlsectionbuttons.h"
#include "interfaceproperty.h"
#include "cboundarylist.h"
#include <cmath>
#include <cstring>
#include <QTranslator>
#include "QtAgIO/formloop.h"
#include <algorithm>


QString caseInsensitiveFilename(QString directory, QString filename);

void FormGPS::setupGui()
{
    FormLoop *formLoop = FormLoop::instance();
    if (!formLoop) {
        qDebug() << "Erreur : `FormLoop::instance()` a retourné nullptr!";
        return;
    } else {
        qDebug() << "✅ FormLoop::instance() est bien instancié.";
    }


    formLoop->setEngine(this);


    qDebug() << "AgIO: All context properties set, loading QML...";

    addImportPath("qrc:/qtagopengps.org/imports/");


    /* Load the QML UI and display it in the main area of the GUI */
    setProperty("title","QtAgOpenGPS");
    addImportPath(":/");

    // Load the translation file
    //QString language = "en"; // Change this variable to "en", "fr" or "ru" as needed
    QString language = settings->value(SETTINGS_menu_language).value<QString>();
    loadTranslation(language);

    connect(this, SIGNAL(objectCreated(QObject*,QUrl)),
            this, SLOT(on_qml_created(QObject*,QUrl)), Qt::QueuedConnection);

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
    rootContext()->setContextProperty("mainForm", this);

    //populate vehicle_list property in vehicleInterface
    vehicle_update_list();
    rootContext()->setContextProperty("vehicleInterface", &vehicle);

    rootContext()->setContextProperty("trk", &trk);
    rootContext()->setContextProperty("tram", &tram);
    qmlRegisterSingletonInstance("Interface", 1, 0, "TracksInterface", &trk);
    qmlRegisterSingletonInstance("Interface", 1, 0, "VehicleInterface", &vehicle);
    qmlRegisterSingletonInstance("Settings", 1, 0, "Settings", settings);

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
    rootContext()->setContextProperty("prefix","");
    //load(QUrl("qrc:/qml/MainWindow.qml"));
    addImportPath(QString("%1/modules").arg(QGuiApplication::applicationDirPath()));
    loadFromModule("AOG", "MainWindow");

    if (rootObjects().isEmpty()) {
        qDebug() << "Error: Failed to load QML!";
        return;
    } else {
        qDebug() << "QML loaded successfully.";
        formLoop->setupGUI();
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

    mainWindow->setProperty("visible",true);

    //have to do this for each Interface and supported data type.
    InterfaceProperty<AOGInterface, int>::set_qml_root(qmlItem(mainWindow, "aog"));
    InterfaceProperty<AOGInterface, uint>::set_qml_root(qmlItem(mainWindow, "aog"));
    InterfaceProperty<AOGInterface, bool>::set_qml_root(qmlItem(mainWindow, "aog"));
    InterfaceProperty<AOGInterface, double>::set_qml_root(qmlItem(mainWindow, "aog"));
    InterfaceProperty<AOGInterface, btnStates>::set_qml_root(qmlItem(mainWindow, "aog"));

    InterfaceProperty<FieldInterface, int>::set_qml_root(qmlItem(mainWindow, "fieldInterface"));
    InterfaceProperty<FieldInterface, uint>::set_qml_root(qmlItem(mainWindow, "fieldInterface"));
    InterfaceProperty<FieldInterface, bool>::set_qml_root(qmlItem(mainWindow, "fieldInterface"));
    InterfaceProperty<FieldInterface, double>::set_qml_root(qmlItem(mainWindow, "fieldInterface"));
    InterfaceProperty<FieldInterface, btnStates>::set_qml_root(qmlItem(mainWindow, "fieldInterface"));

    InterfaceProperty<BoundaryInterface, int>::set_qml_root(qmlItem(mainWindow, "boundaryInterface"));
    InterfaceProperty<BoundaryInterface, uint>::set_qml_root(qmlItem(mainWindow, "boundaryInterface"));
    InterfaceProperty<BoundaryInterface, bool>::set_qml_root(qmlItem(mainWindow, "boundaryInterface"));
    InterfaceProperty<BoundaryInterface, double>::set_qml_root(qmlItem(mainWindow, "boundaryInterface"));
    InterfaceProperty<BoundaryInterface, btnStates>::set_qml_root(qmlItem(mainWindow, "boundaryInterface"));

    InterfaceProperty<RecordedPathInterface, int>::set_qml_root(qmlItem(mainWindow, "recordedPathInterface"));
    InterfaceProperty<RecordedPathInterface, uint>::set_qml_root(qmlItem(mainWindow, "recordedPathInterface"));
    InterfaceProperty<RecordedPathInterface, bool>::set_qml_root(qmlItem(mainWindow, "recordedPathInterface"));
    InterfaceProperty<RecordedPathInterface, double>::set_qml_root(qmlItem(mainWindow, "recordedPathInterface"));
    InterfaceProperty<RecordedPathInterface, btnStates>::set_qml_root(qmlItem(mainWindow, "recordedPathInterface"));

    QMLSectionButtons::set_aog_root(qmlItem(mainWindow, "aog"));
    qmlblockage::set_aog_root(qmlItem(mainWindow, "aog"));

    //initialize interface properties
    isBtnAutoSteerOn = false;
    sentenceCounter = 0;
    manualBtnState = btnStates::Off;
    autoBtnState = btnStates::Off;
    isPatchesChangingColor = false;
    isOutOfBounds = false;
    pn.latStart = 0;
    pn.lonStart = 0;

    //hook up our AOGInterface properties
    QObject *aog = qmlItem(mainWindow, "aog");
    //QObject *vehicleInterface = qmlItem(mainWindow, "vehicleInterface");
    QObject *fieldInterface = qmlItem(mainWindow, "fieldInterface");
    QObject *boundaryInterface = qmlItem(mainWindow, "boundaryInterface");

    //react to UI changing this property
    connect(aog,SIGNAL(sectionButtonStateChanged()), &tool.sectionButtonState, SLOT(onStatesUpdated()));
    //connect(aog,SIGNAL(rowCountChanged()), &tool.blockageRowState, SLOT(onRowsUpdated())); //Dim

    openGLControl = mainWindow->findChild<AOGRendererInSG *>("openglcontrol");
    //This is a bit hackish, but all rendering is done in this item, so
    //we have to give it a way of calling our initialize and draw functions
    openGLControl->setProperty("callbackObject",QVariant::fromValue((void *) this));
    openGLControl->setProperty("initCallback",QVariant::fromValue<std::function<void (void)>>(std::bind(&FormGPS::openGLControl_Initialized, this)));
    openGLControl->setProperty("paintCallback",QVariant::fromValue<std::function<void (void)>>(std::bind(&FormGPS::oglMain_Paint,this)));

    openGLControl->setProperty("samples",settings->value(SETTINGS_display_antiAliasSamples).value<int>());
    openGLControl->setMirrorVertically(true);
    connect(openGLControl,SIGNAL(clicked(QVariant)),this,SLOT(onGLControl_clicked(QVariant)));
    connect(openGLControl,SIGNAL(dragged(int,int,int,int)),this,SLOT(onGLControl_dragged(int,int,int,int)));

    connect(aog,SIGNAL(sectionButtonStateChanged()), &tool.sectionButtonState, SLOT(onStatesUpdated()));

    //on screen buttons
    connect(aog,SIGNAL(zoomIn()), this, SLOT(onBtnZoomIn_clicked()));
    connect(aog,SIGNAL(zoomOut()), this, SLOT(onBtnZoomOut_clicked()));
    connect(aog,SIGNAL(tiltDown()), this, SLOT(onBtnTiltDown_clicked()));
    connect(aog,SIGNAL(tiltUp()), this, SLOT(onBtnTiltUp_clicked()));
    connect(aog,SIGNAL(btn2D()), this, SLOT(onBtn2D_clicked()));
    connect(aog,SIGNAL(btn3D()), this, SLOT(onBtn3D_clicked()));
    connect(aog,SIGNAL(n2D()), this, SLOT(onBtnN2D_clicked()));
    connect(aog,SIGNAL(n3D()), this, SLOT(onBtnN3D_clicked()));
    connect(aog, SIGNAL(isHydLiftOn()), this, SLOT(onBtnHydLift_clicked()));
    connect(aog, SIGNAL(btnResetTool()), this, SLOT(onBtnResetTool_clicked()));
    connect(aog, SIGNAL(btnHeadland()), this, SLOT(onBtnHeadland_clicked()));
    connect(aog, SIGNAL(btnContour()), this, SLOT(onBtnContour_clicked()));
    connect(aog, SIGNAL(btnContourLock()), this, SLOT(onBtnContourLock_clicked()));
    connect(aog, SIGNAL(btnContourPriority(bool)), this, SLOT(onBtnContourPriority_clicked(bool)));
    connect(aog, SIGNAL(isYouSkipOn()), this, SLOT(onBtnYouSkip_clicked()));

    connect(aog, SIGNAL(btnResetSim()), this, SLOT(onBtnResetSim_clicked()));
    connect(aog, SIGNAL(sim_rotate()), this, SLOT(onBtnRotateSim_clicked()));
    connect(aog, SIGNAL(reset_direction()), this, SLOT(onBtnResetDirection_clicked()));

    connect(aog, SIGNAL(centerOgl()), this, SLOT(onBtnCenterOgl_clicked()));

    connect(aog, SIGNAL(deleteAppliedArea()), this, SLOT(onDeleteAppliedArea_clicked()));

    //manual youturn buttons
    connect(aog,SIGNAL(uturn(bool)), this, SLOT(onBtnManUTurn_clicked(bool)));
    connect(aog,SIGNAL(lateral(bool)), this, SLOT(onBtnLateral_clicked(bool)));
    connect(aog,SIGNAL(autoYouTurn()), this, SLOT(onBtnAutoYouTurn_clicked()));
    connect(aog,SIGNAL(swapAutoYouTurnDirection()), this, SLOT(onBtnSwapAutoYouTurnDirection_clicked()));
    connect(aog,SIGNAL(btnResetCreatedYouTurn()), this, SLOT(onBtnResetCreatedYouTurn_clicked()));
    connect(aog,SIGNAL(btnAutoTrack()), this, SLOT(onBtnAutoTrack_clicked()));

    connect(mainWindow, SIGNAL(save_everything()), this, SLOT(FileSaveEverythingBeforeClosingField()));
    //connect(qml_root,SIGNAL(closing(QQuickCloseEvent *)), this, SLOT(fileSaveEverythingBeforeClosingField(QQuickCloseEvent *)));

    //connect settings dialog box
    connect(aog,SIGNAL(settings_reload()), this, SLOT(on_settings_reload()));
    connect(aog,SIGNAL(settings_save()), this, SLOT(on_settings_save()));

    //snap track button

    connect(aog,SIGNAL(snapSideways(double)), this, SLOT(onBtnSnapSideways_clicked(double)));
    connect(aog,SIGNAL(snapToPivot()), this, SLOT(onBtnSnapToPivot_clicked()));

    //vehicle saving and loading
    connect(&vehicle,SIGNAL(vehicle_update_list()), this, SLOT(vehicle_update_list()));
    connect(&vehicle,SIGNAL(vehicle_load(QString)), this, SLOT(vehicle_load(QString)));
    connect(&vehicle,SIGNAL(vehicle_delete(QString)), this, SLOT(vehicle_delete(QString)));
    connect(&vehicle,SIGNAL(vehicle_saveas(QString)), this, SLOT(vehicle_saveas(QString)));

    //field saving and loading
    connect(fieldInterface,SIGNAL(field_update_list()), this, SLOT(field_update_list()));
    connect(fieldInterface,SIGNAL(field_close()), this, SLOT(field_close()));
    connect(fieldInterface,SIGNAL(field_open(QString)), this, SLOT(field_open(QString)));
    qDebug()<< "connect to SIGNAL";
    connect(fieldInterface,SIGNAL(field_new(QString)), this, SLOT(field_new(QString)));
    connect(fieldInterface,SIGNAL(field_new_from(QString,QString,int)), this, SLOT(field_new_from(QString,QString,int)));
    connect(fieldInterface,SIGNAL(field_new_from_KML(QString,QString)), this, SLOT(field_new_from_KML(QString,QString)));
    connect(fieldInterface,SIGNAL(field_delete(QString)), this, SLOT(field_delete(QString)));

    //React to UI changing imuHeading, in order to reset the IMU heading
    connect(aog, SIGNAL(changeImuHeading(double)), &ahrs, SLOT(changeImuHeading(double)));
    connect(aog, SIGNAL(changeImuRoll(double)), &ahrs, SLOT(changeImuRoll(double)));

    //React to UI setting hyd life settings
    connect(aog, SIGNAL(modules_send_238()), this, SLOT(modules_send_238()));
    connect(aog, SIGNAL(modules_send_251()), this, SLOT(modules_send_251()));
    connect(aog, SIGNAL(modules_send_252()), this, SLOT(modules_send_252()));

    connect(aog, SIGNAL(doBlockageMonitoring()), this, SLOT(doBlockageMonitoring()));

    connect(aog, SIGNAL(sim_bump_speed(bool)), &sim, SLOT(speed_bump(bool)));
    connect(aog, SIGNAL(sim_zero_speed()), &sim, SLOT(speed_zero()));
    connect(aog, SIGNAL(sim_reset()), &sim, SLOT(reset()));

    connect(aog, SIGNAL(btnSteerAngleUp()), this, SLOT(btnSteerAngleUp_clicked()));
    connect(aog, SIGNAL(btnSteerAngleDown()), this, SLOT(btnSteerAngleDown_clicked()));
    connect(aog, SIGNAL(btnFreeDrive()), this, SLOT(btnFreeDrive_clicked()));
    connect(aog, SIGNAL(btnFreeDriveZero()), this, SLOT(btnFreeDriveZero_clicked()));
    connect(aog, SIGNAL(btnStartSA()), this, SLOT(btnStartSA_clicked()));

    //boundary signals and slots
    connect(&yt, SIGNAL(outOfBounds()),boundaryInterface,SLOT(setIsOutOfBoundsTrue()));
    connect(boundaryInterface, SIGNAL(calculate_area()), this, SLOT(boundary_calculate_area()));
    connect(boundaryInterface, SIGNAL(update_list()), this, SLOT(boundary_update_list()));
    connect(boundaryInterface, SIGNAL(start()), this, SLOT(boundary_start()));
    connect(boundaryInterface, SIGNAL(stop()), this, SLOT(boundary_stop()));
    connect(boundaryInterface, SIGNAL(add_point()), this, SLOT(boundary_add_point()));
    connect(boundaryInterface, SIGNAL(delete_last_point()), this, SLOT(boundary_delete_last_point()));
    connect(boundaryInterface, SIGNAL(pause()), this, SLOT(boundary_pause()));
    connect(boundaryInterface, SIGNAL(record()), this, SLOT(boundary_record()));
    connect(boundaryInterface, SIGNAL(reset()), this, SLOT(boundary_restart()));
    connect(boundaryInterface, SIGNAL(delete_boundary(int)), this, SLOT(boundary_delete(int)));
    connect(boundaryInterface, SIGNAL(set_drive_through(int, bool)), this, SLOT(boundary_set_drivethru(int,bool)));
    connect(boundaryInterface, SIGNAL(delete_all()), this, SLOT(boundary_delete_all()));


    headland_form.bnd = &bnd;
    headland_form.vehicle = &vehicle;
    headland_form.hdl = &hdl;
    headland_form.tool = &tool;

    headland_form.connect_ui(qmlItem(mainWindow, "headlandDesigner"));
    connect(&headland_form, SIGNAL(saveHeadland()),this,SLOT(headland_save()));
    connect(&headland_form, SIGNAL(timedMessageBox(int,QString,QString)),this,SLOT(TimedMessageBox(int,QString,QString)));

    headache_form.bnd = &bnd;
    headache_form.vehicle = &vehicle;
    headache_form.hdl = &hdl;
    headache_form.tool = &tool;

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
    connect(aog, SIGNAL(btnFlag()), this, SLOT(onBtnFlag_clicked()));
    connect(aog, SIGNAL(btnRedFlag()), this, SLOT(onBtnRedFlag_clicked()));
    connect(aog, SIGNAL(btnGreenFlag()), this, SLOT(onBtnGreenFlag_clicked()));
    connect(aog, SIGNAL(btnYellowFlag()), this, SLOT(onBtnYellowFlag_clicked()));
    connect(aog, SIGNAL(btnDeleteFlag()), this, SLOT(onBtnDeleteFlag_clicked()));
    connect(aog, SIGNAL(btnDeleteAllFlags()), this, SLOT(onBtnDeleteAllFlags_clicked()));
    connect(aog, SIGNAL(btnNextFlag()), this, SLOT(onBtnNextFlag_clicked()));
    connect(aog, SIGNAL(btnPrevFlag()), this, SLOT(onBtnPrevFlag_clicked()));
    connect(aog, SIGNAL(btnCancelFlag()), this, SLOT(onBtnCancelFlag_clicked()));
    connect(aog, SIGNAL(btnRed(double, double, int)), this, SLOT(onBtnRed_clicked(double, double, int)));



    connect(aog, SIGNAL(stopDataCollection()), this, SLOT(StopDataCollection()));
    connect(aog, SIGNAL(startDataCollection()), this, SLOT(StartDataCollection()));
    connect(aog, SIGNAL(resetData()), this, SLOT(ResetData()));
    connect(aog, SIGNAL(applyOffsetToCollectedData(double)), this, SLOT(ApplyOffsetToCollectedData(double)));
    connect(aog, SIGNAL(smartCalLabelClick()), this, SLOT(SmartCalLabelClick()));
    connect(aog, SIGNAL(on_btnSmartZeroWAS_clicked()), this, SLOT(on_btnSmartZeroWAS_clicked()));
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

    isJobStarted = false;

    StartLoopbackServer();
    if (settings->value(SETTINGS_menu_isSimulatorOn).value<bool>() == false) {
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
    openGLControl->update();
}
void FormGPS::onBtnCenterOgl_clicked(){
    qDebug()<<"center ogl";
    camera.panX = 0;
    camera.panY = 0;
    openGLControl->update();
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
    openGLControl->update();
}

void FormGPS::onBtnAgIO_clicked(){
    qDebug()<<"AgIO";
}
void FormGPS::onBtnResetTool_clicked(){
               vehicle.tankPos.heading = vehicle.fixHeading;
               vehicle.tankPos.easting = vehicle.hitchPos.easting + (sin(vehicle.tankPos.heading) * (tool.tankTrailingHitchLength));
               vehicle.tankPos.northing = vehicle.hitchPos.northing + (cos(vehicle.tankPos.heading) * (tool.tankTrailingHitchLength));

               vehicle.toolPivotPos.heading = vehicle.tankPos.heading;
               vehicle.toolPivotPos.easting = vehicle.tankPos.easting + (sin(vehicle.toolPivotPos.heading) * (tool.trailingHitchLength));
               vehicle.toolPivotPos.northing = vehicle.tankPos.northing + (cos(vehicle.toolPivotPos.heading) * (tool.trailingHitchLength));
}
void FormGPS::onBtnHeadland_clicked(){
    qDebug()<<"Headland";
    bnd.isHeadlandOn = !bnd.isHeadlandOn;
               if (bnd.isHeadlandOn)
               {
                   //btnHeadlandOnOff.Image = Properties.Resources.HeadlandOn;
               }
               else
               {
                   //btnHeadlandOnOff.Image = Properties.Resources.HeadlandOff;
               }

               if (vehicle.isHydLiftOn && !bnd.isHeadlandOn) vehicle.isHydLiftOn = false;

               if (!bnd.isHeadlandOn)
               {
                   p_239.pgn[p_239.hydLift] = 0;
                   //btnHydLift.Image = Properties.Resources.HydraulicLiftOff;
               }
}
void FormGPS::onBtnHydLift_clicked(){
    if (bnd.isHeadlandOn)
    {
        vehicle.isHydLiftOn = !vehicle.isHydLiftOn;
        if (vehicle.isHydLiftOn)
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
        vehicle.isHydLiftOn = false;
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

        //if (!yt.isYouTurnBtnOn) btnAutoYouTurn.PerformClick();
    }

}


void FormGPS::onBtnResetDirection_clicked(){
    qDebug()<<"reset Direction";
    // c#Array.Clear(stepFixPts, 0, stepFixPts.Length);

    std::memset(stepFixPts, 0, sizeof(stepFixPts));
    isFirstHeadingSet = false;

    //TODO: most of this should be done in QML
    vehicle.setIsReverse(false);
    TimedMessageBox(2000, "Reset Direction", "Drive Forward > 1.5 kmh");
}
void FormGPS::onBtnFlag_clicked() {

    //TODO if this button is disabled until field is started, we won't
    //need this check.

    if(isGPSPositionInitialized) {
        int nextflag = flagPts.size() + 1;
        QString notes = notes.number(nextflag);
        CFlag flagPt(pn.latitude, pn.longitude, pn.fix.easting, pn.fix.northing, gpsHeading, flagColor, nextflag, notes);
        flagPts.append(flagPt);
        flagsBufferCurrent = false;
        contextFlag->setProperty("ptlat",pn.latitude);
        contextFlag->setProperty("ptlon",pn.longitude);
        contextFlag->setProperty("ptId",nextflag);
        contextFlag->setProperty("ptText",notes);
        //FileSaveFlags();
    }
}
void FormGPS::onBtnRed_clicked(double lat, double lon, int color)
{   qDebug()<<"onBtnRed_clicked";
    if(isGPSPositionInitialized) {
    double east, nort, ptHeading = 0;
    int nextflag = flagPts.size() + 1;
    QString notes = notes.number(nextflag);
    pn.ConvertWGS84ToLocal((double)lat, (double)lon, nort, east);
    CFlag flagPt(lat, lon, east, nort, ptHeading, color, nextflag, notes);
    flagPts.append(flagPt);
    FileSaveFlags();
    }
}

void FormGPS::onBtnContour_clicked(){
    ct.isContourBtnOn = !ct.isContourBtnOn;
    if (ct.isContourBtnOn) {
        guidanceLookAheadTime = 0.5;
    }else{
        //if (ABLine.isBtnABLineOn | curve.isBtnCurveOn){
        //    ABLine.isABValid = false;
        //    curve.isCurveValid = false;
        //}
        guidanceLookAheadTime = settings->value(SETTINGS_as_guidanceLookAheadTime).value<double>();
    }
}

void FormGPS::onBtnContourPriority_clicked(bool isRight){

    ct.isRightPriority = isRight;
    qDebug() << "Contour isRight: " << isRight;
}

void FormGPS::onBtnContourLock_clicked(){
    ct.SetLockToLine();
}

void FormGPS::onBtnTiltDown_clicked(){

    if (camera.camPitch > -59) camera.camPitch = -60;
    camera.camPitch += ((camera.camPitch * 0.012) - 1);
    if (camera.camPitch < -76) camera.camPitch = -76;

    lastHeight = -1; //redraw the sky
    settings->setValue(SETTINGS_display_camPitch, camera.camPitch);
    openGLControl->update();
}

void FormGPS::onBtnTiltUp_clicked(){
    double camPitch = settings->value(SETTINGS_display_camPitch).value<double>();

    lastHeight = -1; //redraw the sky
    camera.camPitch -= ((camera.camPitch * 0.012) - 1);
    if (camera.camPitch > -58) camera.camPitch = 0;

    settings->setValue(SETTINGS_display_camPitch, camera.camPitch);
    openGLControl->update();
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
    openGLControl->update();
}

void FormGPS::onBtnZoomOut_clicked(){
    if (camera.zoomValue <= 20) camera.zoomValue += camera.zoomValue * 0.1;
    else camera.zoomValue += camera.zoomValue * 0.05;
    if (camera.zoomValue > 220) camera.zoomValue = 220;
    camera.camSetDistance = camera.zoomValue * camera.zoomValue * -1;
    camera.SetZoom();

    //todo save to properties
    openGLControl->update();
}

void FormGPS::onBtnRedFlag_clicked()
{   qDebug()<<"onBtnRedFlag_clicked";
    flagColor = 0;
    contextFlag->setProperty("visible",false);
    contextFlag->setProperty("icon","/images/FlagRed.png");
}

void FormGPS::onBtnGreenFlag_clicked()
{   qDebug()<<"onBtnGreenFlag_clicked";
    flagColor = 1;
    contextFlag->setProperty("visible",false);
    contextFlag->setProperty("icon","/images/FlagGrn.png");
}

void FormGPS::onBtnYellowFlag_clicked()
{   qDebug()<<"onBtnYellowFlag_clicked";
    flagColor = 2;
    contextFlag->setProperty("visible",false);
    contextFlag->setProperty("icon","/images/FlagYel.png");
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
    contextFlag->setProperty("visible",false);
    if (flagNumberPicked>0) {
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
    else if (flagPts.size() > 0) {
        flagPts.remove(flagPts.size()-1, 1);

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
void FormGPS::onBtnDeleteAllFlags_clicked()
{   qDebug()<<"onBtnDeleteAllFlag_clicked";
    contextFlag->setProperty("visible",false);
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
    if (flagNumberPicked>0){
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
if (flagNumberPicked>0){
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

     if (!yt.isYouTurnBtnOn)
     {
         //new direction so reset where to put turn diagnostic
         yt.ResetCreatedYouTurn();

         if (!isBtnAutoSteerOn) return;
         yt.isYouTurnBtnOn = true;
         yt.isTurnCreationTooClose = false;
         yt.isTurnCreationNotCrossingError = false;
         yt.ResetYouTurn();
         //mc.autoSteerData[mc.sdX] = 0;
//         mc.machineControlData[mc.cnYouTurn] = 0;
//         btnAutoYouTurn.Image = Properties.Resources.Youturn80;
     }
     else
     {
         yt.isYouTurnBtnOn = false;
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
     //else if (yt.isYouTurnBtnOn)
         //btnAutoYouTurn.PerformClick();
 }

 void FormGPS::onBtnResetCreatedYouTurn_clicked()
 {
     qDebug()<<"ResetCreatedYouTurnd";
     yt.ResetYouTurn();
 }

 void FormGPS::onBtnAutoTrack_clicked()
 {
     trk.isAutoTrack = !trk.isAutoTrack;
     qDebug()<<"isAutoTrack";
 }

void FormGPS::onBtnManUTurn_clicked(bool right)
{
    if (yt.isYouTurnTriggered) {
        yt.ResetYouTurn();
    }else {
        yt.isYouTurnTriggered = true;
        yt.BuildManualYouTurn( right, true, vehicle, trk);
   }
}

void FormGPS::onBtnLateral_clicked(bool right)
{
   yt.BuildManualYouLateral(right, vehicle, trk);
}

void FormGPS::btnSteerAngleUp_clicked(){
    vehicle.driveFreeSteerAngle++;
    if (vehicle.driveFreeSteerAngle > 40) vehicle.driveFreeSteerAngle = 40;

    qDebug()<<"btnSteerAngleUp_clicked";
}
void FormGPS::btnSteerAngleDown_clicked(){
    vehicle.driveFreeSteerAngle--;
    if (vehicle.driveFreeSteerAngle < -40) vehicle.driveFreeSteerAngle = -40;

    qDebug()<<"btnSteerAngleDown_clicked";
}
void FormGPS::btnFreeDrive_clicked(){


    if (vehicle.isInFreeDriveMode)
    {
        //turn OFF free drive mode
        vehicle.isInFreeDriveMode = false;
        vehicle.driveFreeSteerAngle = 0;
    }
    else
    {
        //turn ON free drive mode
        vehicle.isInFreeDriveMode = true;
        vehicle.driveFreeSteerAngle = 0;
    }

    qDebug()<<"btnFreeDrive_clicked";
}
void FormGPS::btnFreeDriveZero_clicked(){
    if (vehicle.driveFreeSteerAngle == 0)
        vehicle.driveFreeSteerAngle = 5;
    else vehicle.driveFreeSteerAngle = 0;

    qDebug()<<"btnFreeDriveZero_clicked";
}

void FormGPS::btnStartSA_clicked(){

 qDebug()<<"btnStartSA_clicked";

    if (!isSA)
    {
        isSA = true;
        startFix = vehicle.pivotAxlePos;
        dist = 0;
        diameter = 0;
        cntr = 0;
        //lblDiameter.Text = "0";
        lblCalcSteerAngleInner = "Drive Steady";
        lblCalcSteerAngleOuter = "Consistent Steering Angle!!";

    }
    else
    {
        isSA = false;
        lblCalcSteerAngleInner = "0.0 + °";
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
    settings->sync();

    loadSettings();
}

void FormGPS::modules_send_238() {
    qDebug() << "Sending 238 message to AgIO";
    p_238.pgn[p_238.set0] = settings->value(SETTINGS_ardMac_setting0).value<int>();
    p_238.pgn[p_238.raiseTime] = settings->value(SETTINGS_ardMac_hydRaiseTime).value<int>();
    p_238.pgn[p_238.lowerTime] = settings->value(SETTINGS_ardMac_hydLowerTime).value<int>();

    p_238.pgn[p_238.user1] = settings->value(SETTINGS_ardMac_user1).value<int>();
    p_238.pgn[p_238.user2] = settings->value(SETTINGS_ardMac_user2).value<int>();
    p_238.pgn[p_238.user3] = settings->value(SETTINGS_ardMac_user3).value<int>();
    p_238.pgn[p_238.user4] = settings->value(SETTINGS_ardMac_user4).value<int>();

    qDebug() << settings->value(SETTINGS_ardMac_user1).value<int>();
    SendPgnToLoop(p_238.pgn);
}
void FormGPS::modules_send_251() {
    //qDebug() << "Sending 251 message to AgIO";
    p_251.pgn[p_251.set0] = settings->value(SETTINGS_ardSteer_setting0).value<int>();
    p_251.pgn[p_251.set1] = settings->value(SETTINGS_ardSteer_setting1).value<int>();
    p_251.pgn[p_251.maxPulse] = settings->value(SETTINGS_ardSteer_maxPulseCounts).value<int>();
    p_251.pgn[p_251.minSpeed] = 5; //0.5 kmh THIS IS CHANGED IN AOG FIXES

    if (settings->value(SETTINGS_as_isConstantContourOn).value<bool>())
        p_251.pgn[p_251.angVel] = 1;
    else p_251.pgn[p_251.angVel] = 0;

    qDebug() << p_251.pgn;
    SendPgnToLoop(p_251.pgn);
}

void FormGPS::modules_send_252() {
    //qDebug() << "Sending 252 message to AgIO";
    p_252.pgn[p_252.gainProportional] = settings->value(SETTINGS_as_Kp).value<int>();
    p_252.pgn[p_252.highPWM] = settings->value(SETTINGS_as_highSteerPWM).value<int>();
    p_252.pgn[p_252.lowPWM] = settings->value(SETTINGS_as_lowSteerPWM).value<int>();
    p_252.pgn[p_252.minPWM] = settings->value(SETTINGS_as_minSteerPWM).value<int>();
    p_252.pgn[p_252.countsPerDegree] = settings->value(SETTINGS_as_countsPerDegree).value<int>();
    p_252.pgn[p_252.wasOffsetHi] = (char)(settings->value(SETTINGS_as_wasOffset).value<int>() >> 8);
    p_252.pgn[p_252.wasOffsetLo] = (char)settings->value(SETTINGS_as_wasOffset).value<int>();
    p_252.pgn[p_252.ackerman] = settings->value(SETTINGS_as_ackerman).value<int>();


    qDebug() << p_252.pgn;
    SendPgnToLoop(p_252.pgn);
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
    sim.latitude = settings->value(SETTINGS_gps_simLatitude).value<double>();
    sim.longitude = settings->value(SETTINGS_gps_simLongitude).value<double>();
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
    if (isJobStarted)
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
                fd.workedAreaTotal = 0;

                //clear the section lists
                for (int j = 0; j < triStrip.count(); j++)
                {
                    //clean out the lists
                    triStrip[j].patchList.clear();
                    triStrip[j].triangleList.clear();
                }
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
        dist = glm::Distance(startFix, vehicle.pivotAxlePos);
        cntr++;
        if (dist > diameter)
        {
            diameter = dist;
            cntr = 0;
        }
        //lblDiameter = diameter.ToString("N2") + " m";
        lblDiameter = locale.toString(diameter, 'g', 3) + tr(" m");
        qDebug()<<"diameter ";
        qDebug()<<diameter;
        if (cntr > 9)
        {
            steerAngleRight = atan(vehicle.wheelbase / ((diameter - vehicle.trackWidth * 0.5) / 2));
            steerAngleRight = glm::toDegrees(steerAngleRight);

            //lblCalcSteerAngleInner = steerAngleRight.ToString("N1") + "°";
            lblCalcSteerAngleInner = locale.toString(steerAngleRight, 'g', 3) + tr("°");
            //lblDiameter.Text = diameter.ToString("N2") + " m";
            lblDiameter = locale.toString(diameter, 'g', 3) + tr(" m");
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

        SendPgnToLoop(p_252.pgn);
        toSend = false;
        counter = 0;
    }

    if (secondCntr++ > 2)
    {
        secondCntr = 0;

        if (tabControl1.SelectedTab == tabPPAdv)
        {
            lblHoldAdv = vehicle.goalPointLookAheadHold.ToString("N1");
            lblAcqAdv = (vehicle.goalPointLookAheadHold * mf.vehicle.goalPointAcquireFactor).ToString("N1");
            lblDistanceAdv = vehicle.goalDistance.ToString("N1");
            lblAcquirePP = lblAcqAdv.Text;
        }
    }

    if (mc.sensorData != -1)
    {
        if (mc.sensorData < 0 || mc.sensorData > 255) mc.sensorData = 0;
        //CExtensionMethods.SetProgressNoAnimation(pbarSensor, mc.sensorData);
        //if (nudMaxCounts.Visible == false)
            //lblPercentFS.Text = ((int)((double)mc.sensorData * 0.3921568627)).ToString() + "%";
        else
            //lblPercentFS.Text = mc.sensorData.ToString();
    }
    */
}

 //Translation fonctions for Desktop and Android
void FormGPS::loadTranslation(const QString &language) {
    QString translationPath;
    #ifdef Q_OS_ANDROID
        translationPath = QString("assets:/i18n/qml_%1.qm").arg(language);
        qDebug() << "Translation load initiated for ANDROID";
    #else
        translationPath = QString("i18n/qml_%1.qm").arg(language);
        qDebug() << "Translation initiated for Desktop";
    #endif
    QCoreApplication::removeTranslator(&translator);
    if (translator.load(translationPath)) {
        qDebug() << "Translation sucessfully loaded from" << translationPath;
        QCoreApplication::installTranslator(&translator);
    } else {
        qDebug() << "Translation not loaded, file not found in :" << translationPath;
    }
}

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
    if (!isBtnAutoSteerOn) return false;
    if (std::abs(vehicle.guidanceLineDistanceOff) > 15000) return false;

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
    int recommendedOffsetAdjustment = GetRecommendedWASOffsetAdjustment(settings->value(SETTINGS_as_countsPerDegree).value<int>());
    int newOffset = settings->value(SETTINGS_as_wasOffset).value<int>() + recommendedOffsetAdjustment;

    // Проверяем новое значение смещения на допустимый диапазон
    if (std::abs(newOffset) > 3900)
    {
        TimedMessageBox(2000, tr("Recommended adjustment {0} exceeds safe range (±50). Please check WAS sensor alignment"), QString(tr("Exceeded Range")) + " " +
                                                                                                                                  QString::number(newOffset, 'f', 1));
        qDebug() << "Smart Zero превысил диапазон:" << newOffset;
        return;
    }

    // Применяем смещение нуля WAS
    settings->setValue(SETTINGS_as_wasOffset, newOffset);

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

