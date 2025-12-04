// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
// Displays the main QML page. All other QML windows are children of this one.
//Loaded by formgps_ui.cpp.
import QtQuick
import QtQuick.Window
import QtQuick.Effects
import QtQuick.Dialogs
// Interface import removed - now QML_SINGLETON
import AOG
import "agio" as AgIOModule


import "interfaces" as Interfaces
import "boundary" as Boundary
import "steerconfig" as SteerConfig
import "config" as ConfigSettings //"Config" causes errors
import "field" as Field
import "tracks" as Tracks
import "components" as Comp
import "wizards" as Wiz

Window {
    //We draw native opengl to this root object
    id: mainWindow
    objectName: "mainWindow"

    // ⚡ Qt 6.8 Modern Pattern: Simple initialization notification
    // No complex property bindings or signal handlers needed

    AOGTheme {
        id: theme
        objectName: "theme"
    }

    AOGInterface {
        id: aogInterface  // Renamed to avoid conflict with global aog
        objectName: "aogInterface"
    }
    //property string prefix: "../.." //make image show in QtDS

    height: theme.defaultHeight
    color: "#0d0d0d"
    width: theme.defaultWidth

    onVisibleChanged: if(SettingsManager.display_isStartFullscreen){
                          mainWindow.showMaximized()
                      }

    Component.onCompleted: {
        // Debug factory function singletons
        console.log("=== FACTORY FUNCTION DEBUG ===")
        console.log("Settings available:", typeof Settings !== 'undefined')
        console.log("TracksInterface available:", typeof TracksInterface !== 'undefined')
        console.log("VehicleInterface available:", typeof VehicleInterface !== 'undefined')
        // AgIOSettings replaced by AgIOService in Phase 4.2
        console.log("AgIOService available:", typeof AgIOService !== 'undefined')

        if (typeof Settings !== 'undefined') {
            console.log("Settings.display_isStartFullscreen:", SettingsManager.display_isStartFullscreen)
        }

        // Force AgIOService factory function call first
        if (typeof AgIOService !== 'undefined') {
            // Force singleton creation via factory function
            var service = AgIOService;  // This should trigger factory function

            console.log("=== AGIO SERVICE TEST ===")
            console.log("GPS Connected:", service.gpsConnected)
            console.log("Latitude:", service.latitude)
            console.log("Longitude:", service.longitude)
            console.log("Vehicle XY:", service.vehicle_xy)
            console.log("Thread test:")
            service.testThreadCommunication()
            console.log("=== END AGIO TEST ===")
        } else {
            console.log("❌ AgIOService NOT available!")
        }

        if (typeof TracksInterface !== 'undefined') {
            console.log("TracksInterface.idx:", TracksInterface.idx)
            console.log("TracksInterface.count:", TracksInterface.count)
            console.log("TracksInterface.model:", TracksInterface.model)
            console.log("TracksInterface identity:", TracksInterface)
        }

        if (typeof VehicleInterface !== 'undefined') {
            console.log("VehicleInterface.isReverse:", VehicleInterface.isReverse)
            console.log("VehicleInterface.vehicleList length:", VehicleInterface.vehicleList ? VehicleInterface.vehicleList.length : "undefined")
            console.log("VehicleInterface identity:", VehicleInterface)
        }

        // AgIOSettings debug removed - replaced by AgIOService in Phase 4.2

        console.log("=== END FACTORY FUNCTION DEBUG ===")

        // Phase 6.0.20 Task 24 Step 3.5 - Test geodetic conversion functions
        console.log("[GEODETIC_TEST] latStart:", aog.latStart, "lonStart:", aog.lonStart )
        if (aog.latStart !== 0 && aog.lonStart !== 0) {
            var local = aog.convertWGS84ToLocal(aog.latStart, aog.lonStart)
            console.log("[GEODETIC_TEST] WGS84->Local origin conversion: northing=", local[0], "easting=", local[1])
            var wgs84 = aog.convertLocalToWGS84(local[0], local[1])
            console.log("[GEODETIC_TEST] Local->WGS84 round-trip: lat=", wgs84[0], "lon=", wgs84[1])
        } else {
            console.log("[GEODETIC_TEST] Field origin not set - skipping conversion test")
        }

        // ⚡ Qt 6.8 Pattern: Component is ready
        console.log("✅ QML MainWindow Component.onCompleted")
        // C++ will be notified via objectCreated signal automatically
    }

    // Phase 6.0.20 Task 24 Step 3.5 - Test when field is loaded
    Connections {
        target: aog
        function onLatStartChanged() {
            if (aog.latStart !== 0 && aog.lonStart !== 0) {
                console.log("[GEODETIC_TEST] Field loaded - latStart:", aog.latStart, "lonStart:", aog.lonStart, "mPerDegreeLat:", aog.mPerDegreeLat)
                var local = aog.convertWGS84ToLocal(aog.latStart, aog.lonStart)
                console.log("[GEODETIC_TEST] WGS84->Local origin: northing=", local[0], "easting=", local[1])
                var wgs84 = aog.convertLocalToWGS84(local[0], local[1])
                console.log("[GEODETIC_TEST] Local->WGS84 round-trip: lat=", wgs84[0], "lon=", wgs84[1])
            }
        }
    }

    // REMOVED: save_everything signal replaced by formGPS.applicationClosing property
    // signal save_everything(bool saveVehicle)

    function close() {
        if (areWindowsOpen()) {
            timedMessage.addMessage(2000,qsTr("Some windows are open. Close them first."))
            console.log("some windows are open. close them first")
            return
        }
        if (aog.autoBtnState + aog.manualBtnState  > 0) {
            timedMessage.addMessage(2000,qsTr("Section Control on. Shut off Section Control."))
            close.accepted = false
            console.log("Section Control on. Shut off Section Control.")
            return
        }
        if (mainWindow.visibility !== (Window.FullScreen) && mainWindow.visibility !== (Window.Maximized)){
            SettingsManager.window_size = ((mainWindow.width.toString() + ", "+  (mainWindow.height).toString()))
        }

        if (aog.isJobStarted) {
            closeDialog.visible = true
            close.accepted = false
            console.log("job is running. close it first")
            return
        }
        Qt.quit()
    }
    function areWindowsOpen() {
        if (config.visible === true) {
            console.log("config visible")
            return true
        }
        else if (headlandDesigner.visible === true) {
            console.log("headlandDesigner visible")
            return true
        }
        else if (headacheDesigner.visible === true) {
            console.log("headacheDesigner visible")
            return true
        }
        else if (steerConfigWindow.visible === true) {
            console.log("steerConfigWindow visible")
            return true
        }
        /*
        else if (abCurvePicker.visible === true) {
            console.log("abCurvePicker visible")
            return true
        }
        else if (abLinePicker.visible === true) {
            console.log("abLinePicker visible")
            return true
        }*/
        else if (tramLinesEditor.visible === true) {
            console.log("tramLinesEditor visible")
            return true
        }
        else if (lineEditor.visible === true) {
            console.log("lineEditor visible")
            return true
        }
        //if (boundaryMenu.visible == true) return false
        //if (lineDrawer.visible) return false
        //if (lineNudge.acive) return false
        //if (refNudge.acive) return false
        else if (setSimCoords.visible === true) {
            console.log("setSimCoords visible")
            return true
        }
        /* Must implement the new track dialog
        else if (trackNew.visible === true) {
            console.log("trackNew visible")
            return true
        }*/
        else if (fieldNew.visible === true) {
            console.log("FieldNew visible")
            return true
        }
        //if (fieldFromKML.visible) return false
        else if (fieldOpen.visible === true) return true
        //if (contextFlag.visible == true) return false
        else return false
    }


    Interfaces.FieldInterface {
        id: fieldInterface
        objectName: "fieldInterface"
    }

    Interfaces.BoundaryInterface {
        id: boundaryInterface
        objectName: "boundaryInterface"
    }

    Interfaces.RecordedPathInterface {
        id: recordedPathInterface
        objectName: "recordedPathInterface"
    }

    Comp.TimedMessage {
        //This is a popup message that dismisses itself after a timeout
        id: timedMessage
        objectName: "timedMessage"
    }

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    MainTopPanel{
        id: topLine
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
    }


    AOGRenderer {
        id: glcontrolrect
        objectName: "openglcontrol"

        anchors.top: topLine.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        //z: -1

        //for moving the center of the view around
        // ✅ PHASE 6.3.0: shiftX/shiftY are now Q_PROPERTY in C++ AOGRendererInSG class
        // shiftX: 0 //-1 left to 1 right (default value set in C++)
        // shiftY: 0 //-1 down to 1 up (default value set in C++)

        signal clicked(var mouse)
        signal dragged(int fromX, int fromY, int toX, int toY)
        signal zoomOut()
        signal zoomIn()

        MouseArea {
            id: mainMouseArea
            anchors.fill: parent

            property int fromX: 0
            property int fromY: 0
            property Matrix4x4 clickModelView
            property Matrix4x4 clickProjection
            property Matrix4x4 panModelView
            property Matrix4x4 panProjection

            onClicked: function(mouse) {
                parent.clicked(mouse)
            }

            onPressed: if(aogInterface.panMode){
                           //save a copy of the coordinates
                           fromX = mouseX
                           fromY = mouseY
                       }

            onPositionChanged: if(aogInterface.panMode){
                                   parent.dragged(fromX, fromY, mouseX, mouseY)
                                   fromX = mouseX
                                   fromY = mouseY
                               }

            onWheel:(wheel)=>{
                        if (wheel.angleDelta.y > 0) {
                            aog.zoomIn() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
                        } else if (wheel.angleDelta.y <0 ) {
                            aog.zoomOut() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
                        }
                    }

            Image {
                id: reverseArrow
                x: aog.vehicle_xy.x - 150
                y: aog.vehicle_xy.y - height
                width: 70 * theme.scaleWidth
                height: 70 * theme.scaleHeight
                source: prefix + "/images/Images/z_ReverseArrow.png"
                visible: VehicleInterface.isReverse || VehicleInterface.isChangingDirection
            }
            MouseArea{
                //button that catches any clicks on the vehicle in the GL Display
                id: resetDirection
                onClicked: {
                    aog.resetDirection() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
                    console.log("reset direction")
                }
                propagateComposedEvents: true
                x: aog.vehicle_bounding_box.x
                y: aog.vehicle_bounding_box.y
                width: aog.vehicle_bounding_box.width
                height: aog.vehicle_bounding_box.height
                onPressed: (mouse)=>{
                               aog.resetDirection() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
                               console.log("pressed")
                               mouse.accepted = false

                           }
            }
            //            Rectangle{
            //              // to show the reset vehicle direction button for testing purposes
            //                color: "blue"
            //                anchors.fill: resetDirection
            //            }

        } // MouseArea

    }

    Rectangle{
        id: noGPS
        anchors.fill: glcontrolrect
        color: "#0d0d0d"
        visible: aog.sentenceCounter> 29
        onVisibleChanged: if(visible){
                              console.log("no gps now visible")
                          }

        Image {
            id: noGPSImage
            source: prefix + "/images/Images/z_NoGPS.png"
            anchors.centerIn: parent
            anchors.margins: 200
            visible: noGPS.visible
            height: parent.height /2
            width: height
        }
    }

    Item {//item to hold all the main window buttons. Fills all of main screen

        id: buttonsArea
        anchors.top: parent.top
        anchors.topMargin: 2 //TODO: convert to scalable //do we need?
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        MainLeftColumn {
            id: leftColumn
            anchors.top: parent.top
            anchors.topMargin: topLine.height
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.leftMargin: 6
        }

        Speedometer {
            anchors.top: parent.top
            anchors.right: rightColumn.left
            anchors.topMargin: topLine.height + 10
            anchors.margins: 10
            visible: SettingsManager.menu_isSpeedoOn

            speed: Utils.speed_to_unit(aog.speedKph)
        }

        SteerCircle { //the IMU indicator on the bottom right -- Called the "SteerCircle" in AOG
            anchors.bottom: bottomButtons.top
            anchors.right: rightColumn.left
            anchors.margins: 10
            visible: true
            rollAngle: aog.imuRollDegrees
            // Phase 6.0.20 Task 24 Step 3.2: Use aog.steerModuleConnectedCounter instead of aogInterface
            steerColor: (aog.steerModuleConnectedCounter > 30 ?
                             "#f0f218f0" :
                             (aog.steerSwitchHigh === true ?
                                  "#faf80007" :
                                  (aog.isBtnAutoSteerOn === true ?
                                       "#f80df807" : "#f0f2c007")))

        }

        MainRightColumn{
            id: rightColumn
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: topLine.height + 6
            anchors.rightMargin: 6
        }


        Comp.IconButton {
            id: btnContourPriority
            anchors.top: parent.top
            anchors.topMargin: theme.buttonSize + 3
            anchors.right: rightColumn.left
            anchors.rightMargin: 3
            checkable: true
            isChecked: true
            visible: false
            icon.source: prefix + "/images/ContourPriorityLeft.png"
            iconChecked: prefix + "/images/ContourPriorityRight.png"
            onClicked: aog.contourPriority(checked) // Qt 6.8 MODERN: Direct Q_INVOKABLE call
        }

        MainBottomRow{
            id: bottomButtons
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.leftMargin: leftColumn.width + 3
            anchors.right: parent.right
            anchors.rightMargin: rightColumn.width + 3
        }

        //----------------inside buttons-----------------------
        Item{
            //plan to move everything on top of the aogRenderer that isn't
            //in one of the buttons columns
            id: inner
            anchors.left: leftColumn.right
            anchors.top: parent.top
            anchors.topMargin: topLine.height
            anchors.right: rightColumn.left
            anchors.bottom: bottomButtons.top
            visible: !noGPS.visible
            Comp.IconButtonTransparent{ //button to pan around main GL
                implicitWidth: 50
                implicitHeight: 50 * theme.scaleHeight
                checkable: true
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 30
                icon.source: prefix + "/images/Pan.png"
                iconChecked: prefix + "/images/SwitchOff.png"
                onClicked: aogInterface.setPanMode(!aogInterface.panMode) // Qt 6.8: Local AOGInterface property
            }
            Image{
                id: hydLiftIndicator
                property bool isDown: VehicleInterface.hydLiftDown
                visible: false
                source: prefix + "/images/Images/z_Lift.png"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 80 * theme.scaleWidth
                height: 130 * theme.scaleHeight
                onIsDownChanged: {
                    if(!isDown){
                        hydLiftIndicatorColor.colorizationColor = "#00F200"
                        hydLiftIndicatorColor.rotation = 0
                    }else{
                        hydLiftIndicatorColor.rotation = 180
                        hydLiftIndicatorColor.colorizationColor = "#F26600"
                    }
                }
            }
            MultiEffect{
                id: hydLiftIndicatorColor
                anchors.fill: hydLiftIndicator
                visible: bottomButtons.hydLiftIsOn
                colorizationColor:"#F26600"
                colorization: 1.0
                source: hydLiftIndicator
            }

            Comp.OutlineText{
                id: simulatorOnText
                visible: SettingsManager.menu_isSimulatorOn
                anchors.top: parent.top
                anchors.topMargin: lightbar.height+ 10
                anchors.horizontalCenter: lightbar.horizontalCenter
                font.pixelSize: 30
                color: "#cc5200"
                text: qsTr("Simulator On")
            }

            Comp.OutlineText{
                id: ageAlarm //Lost RTK count up display
                property int age: aog.age
                visible: SettingsManager.gps_isRTK
                anchors.top: simulatorOnText.bottom
                anchors.topMargin: 30
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Lost RTK")
                font.pixelSize: 65
                color: "#cc5200"
                onAgeChanged: {
                    if (age < 20)
                        text = ""
                    else if (age> 20 && age < 60)
                        text = qsTr("Age: ")+age
                    else
                        text = "Lost RTK"
                }
                onTextChanged: if (ageAlarm.text.length > 0)
                                   console.log("rtk alarm sound")

            }

            AutoUturnBtn{
                id: autoTurn // where the auto turn button and distance to turn are held
                anchors.top:gridTurnBtns.top
                anchors.right: parent.right
                anchors.rightMargin: 200
                width: 100 * theme.scaleWidth
                height: 100 * theme.scaleHeight
            }

            ManualTurnBtns{
                id: gridTurnBtns
                anchors.top: lightbar.bottom
                anchors.left: parent.left
                anchors.topMargin: 30
                anchors.leftMargin: 150
            }

            LightBar {
                id: lightbar
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.margins: 5
                dotDistance: aog.avgPivDistance / 10 //avgPivotDistance is averaged
                visible: (aog.offlineDistance != 32000 &&
                          SettingsManager.menu_isLightBarOn) ?
                             true : false
            }

            TrackNum {
                id: tracknum
                anchors.top: lightbar.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.margins: 5

                font.pixelSize: 24

                //only use dir names for AB Lines with heading
                useDirNames: (aogInterface.currentABLine > -1)
                currentTrack: TracksInterface.idx

                trackHeading: aogInterface.currentABLine > -1 ?
                                  aogInterface.currentABLine_heading :
                                  0

                visible: (SettingsManager.display_topTrackNum &&
                          ((aogInterface.currentABLine > -1) ||
                           (aogInterface.currentABCurve > -1)))
                //TODO add contour
            }

            TramIndicators{
                id: tramLeft
                anchors.top: tracknum.bottom
                anchors.margins: 30
                anchors.left: parent.horizontalCenter
                visible: SettingsManager.feature_isTramOn
            }
            TramIndicators{
                id: tramRight
                anchors.top: tracknum.bottom
                anchors.margins: 30
                anchors.right: parent.horizontalCenter
                visible: SettingsManager.feature_isTramOn
            }

            //Components- this is where the windows that get displayed over the
            //ogl get instantiated.
            Field.FieldData{ //window that displays field acreage and such
                id: fieldData
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                visible: false
            }
            GPSData{ //window that displays GPS data
                id: gpsData
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                visible: false
            }
            BlockageData{ //window that displays GPS data
                id: blockageData
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                visible: AgIOService.blockageConnected && SettingsManager.seed_blockageIsOn
            }

            SimController{
                id: simBarRect
                //z: 2
                anchors.bottom: timeText.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 8
                visible: SettingsManager.menu_isSimulatorOn
                height: 60 * theme.scaleHeight
                onHeightChanged: anchors.bottomMargin = (8 * theme.scaleHeight)
            }
            RecPath{// recorded path menu
                id: recPath
                visible: false
            }

            Comp.OutlineText{ //displays time on bottom right of GL
                id: timeText
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.rightMargin: (50 * theme.scaleWidth)
                font.pixelSize: 20
                color: "#cc5200"
                property string currentTime: "HH:mm:ss"
                text: currentTime

                Timer{
                    id: timer
                    interval: 1000
                    repeat: true
                    running: aog.rawHz>10?
                    onTriggered: timeText.text = Qt.formatTime(new Date(), "HH:mm:ss")
                }

                // Connections {
                //     target: timer
                //     onTriggered: {
                //         if (timeText.visible) {
                //             timeText.currentTime = Qt.formatTime(new Date(), "HH:mm:ss")
                //         }
                //     }
                // }

            }
            Comp.SectionButtons {
                id: sectionButtons
                visible: aog.isJobStarted ? true : false
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: simBarRect.top
                anchors.bottomMargin: 8
                height: 40 * theme.scaleHeight
                width: 660  * theme.scaleWidth
                //onHeightChanged: anchors.bottomMargin = (bottomButtons.height + simBarRect.height + (24 * theme.scaleHeight))
            }
            Comp.BlockageRows {
                id: blockageRows
                visible: AgIOService.blockageConnected && SettingsManager.seed_blockageIsOn
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                height: 120 * theme.scaleHeight
                //width: 800  * theme.scaleWidth

            }
            DisplayButtons{ // window that shows the buttons to change display. Rotate up/down, day/night, zoom in/out etc. See DisplayButtons.qml
                id: displayButtons
                width: childrenRect.width + 10
                height: childrenRect.height + 10
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.top: parent.top
                anchors.topMargin: 20
                visible: false
                z:1
            }

            Tracks.TrackButtons{
                id: trackButtons
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 20
                visible: false
                z:1
            }
            Comp.IconButtonTransparent{ //button on bottom left to show/hide the bottom and right buttons
                id: toggleButtons
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 25
                visible: aog.isJobStarted
                width: 45 * theme.scaleWidth
                height: 25 * theme.scaleHeight
                icon.source: prefix + "/images/MenuHideShow.png"
                onClicked: if(leftColumn.visible){
                               leftColumn.visible = false
                           }else{
                               leftColumn.visible = true
                           }
            }
            Compass{
                id: compass
                anchors.top: parent.top
                anchors.right: zoomBtns.left
                heading: -Utils.radians_to_deg(aog.fusedHeading)
                visible: SettingsManager.menu_isCompassOn
            }
            Column{
                id: zoomBtns
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 100
                width: children.width
                Comp.IconButton{
                    implicitWidth: 30 * theme.scaleWidth
                    implicitHeight: 30 * theme.scaleHeight
                    radius: 0
                    icon.source: prefix + "/images/ZoomIn48.png"
                    onClicked: aog.zoomIn() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
                }
                Comp.IconButton{
                    implicitWidth: 30 * theme.scaleWidth
                    implicitHeight: 30 * theme.scaleHeight
                    radius: 0
                    icon.source: prefix + "/images/ZoomOut48.png"
                    onClicked: aog.zoomOut() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
                }
            }
        }



        Comp.SliderCustomized { //quick dirty hack--the up and down buttons change this value, so the speed changes
            id: speedSlider
            //anchors.bottom: bottomButtons.top
            //            anchors.bottomMargin: 3
            //            anchors.left:bottomButtons.left
            //            anchors.leftMargin: 3
            from: -80
            to: 300
            value: 0
            visible: false
        }

        StartUp{ //splash we show on startup
            id: startUp
            z:10
            //visible: true
            visible: false  //no reason to look at this until release
        }


        Field.FieldToolsMenu {
            id: fieldTools
            visible: false
        }
        Field.FieldMenu {
            id: fieldMenu
            objectName: "slideoutMenu"
            visible: false
        }
        ToolsWindow {
            id: toolsMenu
            visible: false
        }
        HamburgerMenu{ // window behind top left on main GL
            id: hamburgerMenu
            visible: false
        }

        ConfigSettings.Config {
            id:config
            x: 0
            y: 0
            width: parent.width
            height: parent.height
            visible:false

            onAccepted: {
                console.debug("accepting settings and closing window.")
                aog.settings_save()
                aog.settingsReload() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
            }
            onRejected: {
                console.debug("rejecing all settings changes.")
                aog.settings_revert()
                aog.settingsReload() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
            }

        }
        HeadlandDesigner{
            id: headlandDesigner
            objectName: "headlandDesigner"
            //anchors.horizontalCenter: parent.horizontalCenter
            //anchors.verticalCenter: parent.verticalCenter
            visible: false
        }
        HeadAcheDesigner{
            id: headacheDesigner
            objectName: "headacheDesigner"
            //anchors.horizontalCenter: parent.horizontalCenter
            //anchors.verticalCenter: parent.verticalCenter
            visible: false
        }
        SteerConfig.SteerConfigWindow {
            id:steerConfigWindow
            visible: false
        }
        SteerConfig.SteerConfigSettings{
            id: steerConfigSettings
            visible: false
        }
        /*
        ABCurvePicker{
            id: abCurvePicker
            objectName: "abCurvePicker"
            visible: false
        }
        ABLinePicker{
            id: abLinePicker
            objectName: "abLinePicker"
            visible: false
        }*/
        TramLinesEditor{
            id: tramLinesEditor
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 150
            anchors.topMargin: 50
            visible: false
        }
        LineEditor{
            id: lineEditor
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 150
            anchors.topMargin: 50
            visible: false
        }
        Boundary.BoundaryMenu{
            id: boundaryMenu
            visible: false
        }

        Tracks.LineDrawer {//window where lines are created off field boundary/edited
            id:lineDrawer
            //anchors.horizontalCenter: parent.horizontalCenter
            //anchors.bottom: parent.bottom
            //height: 768
            //width:1024
            visible:false
        }
        Tracks.LineNudge{
            id: lineNudge
            visible: false
        }
        Tracks.RefNudge{
            id: refNudge
            visible: false
        }
        SetSimCoords{
            id: setSimCoords
            anchors.fill: parent
        }
        ConfigSettings.SetColors{
            id: setColors
            anchors.fill: parent
        }
        Tracks.TrackNewButtons{
            id: trackNewButtons
            visible: false
        }
        Tracks.TrackNewSet{
            id: trackNewSet
            anchors.fill: parent
        }

        Tracks.TrackList{
            id: trackList
        }

        Tracks.TracksNewAddName{
            id: trackAddName
        }
        Wiz.ChartSteer{
            id: steerCharta
            height: 300  * theme.scaleHeight
            width: 400  * theme.scaleWidth
            xval1: aog.steerAngleActual
            xval2: aog.steerAngleSet
            axismin: -10
            axismax: 10
            lineName1:"Actual"
            lineName2: "SetPoint"
            chartName: qsTr("Steer Chart")
            visible: false
            function show(){
                steerCharta.visible = true
            }
        }

        Wiz.ChartSteer{
            id: xteCharta
            height: 300  * theme.scaleHeight
            width: 400  * theme.scaleWidth
            xval1: aog.lblmodeActualXTE
            xval2: Number(aog.dataSteerAngl)
            axismin: -100
            axismax: 100
            lineName1:"XTE"
            lineName2:"HE"
            chartName: qsTr("XTE Chart")
            visible: false
            function show(){
                xteCharta.visible = true
            }
        }

        Wiz.ChartSteer{
            id: headingCharta
            height: 300  * theme.scaleHeight
            width: 400  * theme.scaleWidth
            xval1: aog.heading  // Rectangle Pattern: direct property access
            xval2: aog.imuHeading > 360 ? 0 : aog.imuHeading  // Show real IMU heading, 0 if invalid
            axismin: -10
            axismax: 10
            lineName1:"Fix2fix"
            lineName2:"IMU"
            chartName: qsTr("Heading Chart")
            visible: false
            function show(){
                headingCharta.visible = true
            }
        }

        Wiz.Camera{
            id: cam1
            height: 300  * theme.scaleHeight
            width: 400  * theme.scaleWidth
        }
        Wiz.WasWizard{
            id: wasWizard
            height: 300  * theme.scaleHeight
            width: 400  * theme.scaleWidth
            visible: false
            function show(){
                wasWizard.visible = true
            }
        }

        Rectangle{//show "Are you sure?" when close button clicked
            id: closeDialog
            width: 500 * theme.scaleWidth
            height: 100 * theme.scaleHeight
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            color: aogInterface.backgroundColor
            border.color: aogInterface.blackDayWhiteNight
            border.width: 2
            visible: false
            Comp.IconButtonText{
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color1: "transparent"
                color2: "transparent"
                color3: "transparent"
                icon.source: prefix + "/images/back-button.png"
                onClicked: parent.visible = false
            }
            Comp.IconButtonText{
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                color1: "transparent"
                color2: "transparent"
                color3: "transparent"
                icon.source: prefix + "/images/ExitAOG.png"
                onClicked: {
                    formGPS.applicationClosing = true  // Save vehicle when exiting app (Qt 6.8 binding)
                    Qt.quit()
                }
            }
        }
        Item{
            id: windowsArea      //container for declaring all the windows
            anchors.fill: parent //that can be displayed on the main screen
            Field.FieldFromExisting{
                id: fieldFromExisting
                x: 0
                y: 0
            }
            Field.FieldNew{
                id: fieldNew
            }
            Field.FieldFromKML{
                id: fieldFromKML
                x: 100
                y: 75
            }
            Field.FieldOpen{
                id: fieldOpen
                x: 100
            }
            Field.Flags{
                id: flags
            }
            Field.FlagLatLon{
                id: flagLatLon
            }

            y: 75
        }
    }


    Rectangle {
        id: contextFlag
        objectName: "contextFlag"
        width: Math.max(50, childrenRect.width + 10)
        height: Math.max(50, childrenRect.height + 10)
        color: "#bf163814"
        visible: false
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: theme.buttonSize + 10
        //anchors.topMargin: btnFlag.y
        border.width: 2
        border.color: "#c3ecc0"
        property string icon: "/images/FlagRed.png";
        property double ptlat: 0
        property double ptlon: 0
        property double ptId: 0
        property string ptText: ""


        Grid {
            id: contextFlagGrid
            spacing: 5
            anchors.top: parent.top
            anchors.topMargin: 5
            anchors.left: parent.left
            anchors.leftMargin: 5

            width: childrenRect.width
            height: childrenRect.height

            columns: 5
            flow: Grid.LeftToRight

            Comp.IconButton {
                id: redFlag
                objectName: "btnRedFlag"
                icon.source: prefix + "/images/FlagRed.png";
                onClicked: aog.redFlag() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
            }
            Comp.IconButton {
                id: greenFlag
                objectName: "btnGreenFlag"
                icon.source: prefix + "/images/FlagGrn.png";
                onClicked: aog.greenFlag() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
            }
            Comp.IconButton {
                id: yellowFlag
                objectName: "btnYellowFlag"
                icon.source: prefix + "/images/FlagYel.png";
                onClicked: aog.yellowFlag() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
            }
            Comp.IconButton {
                id: deleteFlag
                objectName: "btnDeleteFlag"
                icon.source: prefix + "/images/FlagDelete.png"
                //enabled: false
                onClicked: aog.deleteFlag() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
            }
            Comp.IconButton {
                id: deleteAllFlags
                objectName: "btnDeleteAllFlags"
                icon.source: prefix + "/images/FlagDeleteAll.png"
                //enabled: false
                onClicked: aog.deleteAllFlags() // Qt 6.8 MODERN: Direct Q_INVOKABLE call
            }
        }
        /********************************dialogs***********************/
        ColorDialog{//color picker
            id: cpSectionColor
            onSelectedColorChanged: {

                //just use the Day setting. AOG has them locked to the same color anyways
                SettingsManager.display_colorSectionsDay = cpSectionColor.selectedColor;

                //change the color on the fly. In AOG, we had to cycle the sections off
                //and back on. This does for us.
                if(btnSectionManual){
                    btnSectionManual.clicked()
                    btnSectionManual.clicked()
                }else if(btnSectionAuto){
                    btnSectionAuto.clicked()
                    btnSectionAuto.clicked()
                }
            }
        }

        CloseAOG{
            id: closeAOG
        }
    }


    AgIOModule.AgIO {
          id: mainWindowAgIO
    }
}

