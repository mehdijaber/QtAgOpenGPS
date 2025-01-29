// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// Settings in the expanded steer config. **Not the sliders**
import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts
import QtQuick.Dialogs

import ".."
import "../components"


Window{
    id: steerConfig
    height: mainWindow.height
    title: qsTr("Auto Steer Config")
    visible: true
    width: mainWindow.width


    onVisibleChanged:{
        if(visible){
            settingsArea.load_settings()
            console.log("Settings loaded")
        }
    }

    Rectangle{//background
        anchors.fill: parent
        color: "lightgray"
    }

    ButtonGroup{
		buttons: settingsBtns.children
	}

	RowLayout{
		id: settingsBtns
		spacing: 3 * theme.scaleWidth
		width: parent.width
        anchors.top: parent.top
        anchors.topMargin: 20 * theme.scaleHeight
        SteerConfigTopButtons{
			id: sensorsBtn
            buttonText: "Sensors"
            icon.source: "../../images/Config/ConD_Speedometer.png"
            implicitWidth: parent.width /4 -4
            checked: true //because one has to be to start things off
        }
        SteerConfigTopButtons{
			id: configBtn
			buttonText: "Config"
            icon.source: "../../images/Config/ConS_Pins.png"
        }
        SteerConfigTopButtons{
			id: settingsBtn
			buttonText: "Settings"
            icon.source: "../../images/Config/ConS_ImplementConfig.png"
        }
        SteerConfigTopButtons{
			id: steerSettingsBtn
			buttonText: "Steer Settings"
            icon.source: "../../images/Config/ConS_ImplementConfig.png"
        }
	}
	Item{
		id: settingsArea
		anchors.left: parent.left
		anchors.right: parent.right
		anchors.top: settingsBtns.bottom
		anchors.bottom: bottomRightButtons.top
		anchors.topMargin: 10 * theme.scaleHeight
		anchors.bottomMargin: 10 * theme.scaleHeight
		anchors.leftMargin: 10 * theme.scaleWidth
		anchors.rightMargin: 10 * theme.scaleWidth

		function load_settings(){ 
			if (visible) {
                var sett = settings.setArdSteer_setting0

				if ((sett & 1) == 0) chkInvertWAS.checked = false;
				else chkInvertWAS.checked = true;

				if ((sett & 2) == 0) chkSteerInvertRelays.checked = false;
				else chkSteerInvertRelays.checked = true;

				if ((sett & 4) == 0) chkInvertSteer.checked = false;
				else chkInvertSteer.checked = true;

				//if ((sett & 8) == 0) cboxConv.currentText = "Differential";
				//else cboxConv.currentText = "Single";
				if ((sett & 8) == 0) cboxConv.currentIndex = 1;
				else cboxConv.currentIndex = 0;

				//if ((sett & 16) == 0) cboxMotorDrive.currentText = "IBT2";
				//else cboxMotorDrive.currentText = "Cytron";
				if ((sett & 16) == 0) cboxMotorDrive.currentIndex = 1;
				else cboxMotorDrive.currentIndex = 0;

				if ((sett & 32) == 32) cboxSteerEnable.currentIndex = 1;
				else if ((sett & 64) == 64) cboxSteerEnable.currentIndex = 2;
				else cboxSteerEnable.currentIndex = 0;

				if ((sett & 128) == 0){
					console.log("encoder not checked")
					cboxEncoder.checked = false;
				}
				else {
					console.log("encoder set to checked") 
					cboxEncoder.checked = true;
				}

                nudMaxCounts.value = settings.setArdSteer_maxPulseCounts;
                hsbarSensor.value = settings.setArdSteer_maxPulseCounts;

                sett = settings.setArdSteer_setting1;

                if ((sett & 1) == 0) cboxDanfoss.checked = false;
                else cboxDanfoss.checked = true;

                //if ((sett & 8) == 0) cboxXY.Text = "X";
                //else cboxXY.Text = "Y";
                if ((sett & 8) == 0) cboxXY.currentIndex = 0;
                else cboxXY.currentIndex = 1;

                if ((sett & 2) == 0) cboxPressureSensor.checked = false;
                else cboxPressureSensor.checked = true;

                if ((sett & 4) == 0) cboxCurrentSensor.checked = false;
                else cboxCurrentSensor.checked = true;

                /*the display logic that went here has been moved to the individual
                 *component files. They are not needed in QML
                */
            }
        }

        function save_settings() {
            var set = 1;
            var reset = 2046;
            var sett = 0;

            if (chkInvertWAS.checked) sett |= set;
            else sett &= reset;

            set <<= 1;
            reset <<= 1;
            reset += 1;
            if (chkSteerInvertRelays.checked) sett |= set;
            else sett &= reset;

            set <<= 1;
            reset <<= 1;
            reset += 1;
            if (chkInvertSteer.checked) sett |= set;
            else sett &= reset;

            set <<= 1;
            reset <<= 1;
            reset += 1;
            if (cboxConv.currentText === "Single") sett |= set;
            else sett &= reset;

            set <<= 1;
            reset <<= 1;
            reset += 1;
            if (cboxMotorDrive.currentText === "Cytron") sett |= set;
            else sett &= reset;

            set <<= 1;
            reset <<= 1;
            reset += 1;
            if (cboxSteerEnable.currentText === "Switch") sett |= set;
            else sett &= reset;

            set <<= 1;
            reset <<= 1;
            reset += 1;
            if (cboxSteerEnable.currentIndex === 2) sett |= set;
            else sett &= reset;

            set <<= 1;
            reset <<= 1;
            reset += 1;
            if (cboxEncoder.checked){
                console.log("encoder checked")
                sett |= set;
            }
            else {
                console.log("encoder not checked")
                sett &= reset;
            }

            //set = (set << 1);
            //reset = (reset << 1);
            //reset = (reset + 1);
            //if ( ) sett |= set;
            //else sett &= reset;

            settings.setArdSteer_setting0 = sett;
            settings.setArdMac_isDanfoss = cboxDanfoss.checked;

            if (cboxCurrentSensor.checked || cboxPressureSensor.checked)
            {
                settings.setArdSteer_maxPulseCounts = hsbarSensor.value;
            }
            else
            {
                settings.setArdSteer_maxPulseCounts = nudMaxCounts.value;
            }

            // Settings1
            set = 1;
            reset = 2046;
            sett = 0;

            if (cboxDanfoss.checked) sett |= set;
            else sett &= reset;

            set <<= 1;
            reset <<= 1;
            reset += 1;
            if (cboxPressureSensor.checked) sett |= set;
            else sett &= reset;

            //bit 2
            set <<= 1;
            reset <<= 1;
            reset += 1;
            if (cboxCurrentSensor.checked) sett |= set;
            else sett &= reset;

            //bit 3
            set <<= 1;
            reset <<= 1;
            reset += 1;
            if (cboxXY.currentIndex === 1) sett |= set;
            else sett &= reset;

            settings.setArdSteer_setting1 = sett;

            //Properties.Settings.Default.Save(); not sure what happens here?? David

            //set pgns
            //signal handler will do the following:
            /*
                   mf.p_251.pgn[mf.p_251.set0] = Properties.Settings.Default.setArdSteer_setting0;
                   mf.p_251.pgn[mf.p_251.set1] = Properties.Settings.Default.setArdSteer_setting1;
                   mf.p_251.pgn[mf.p_251.maxPulse] = Properties.Settings.Default.setArdSteer_maxPulseCounts;
                   mf.p_251.pgn[mf.p_251.minSpeed] = 5; //0.5 kmh THIS IS CHANGED IN AOG FIXES

                   if (settings.setAS_isConstantContourOn)
                   mf.p_251.pgn[mf.p_251.angVel] = 1;
                   else mf.p_251.pgn[mf.p_251.angVel] = 0;
                   */
            aog.modules_send_251()

            unsaved.visible = false;
        }
        function reset_all() {
            timedMessage.addMessage(2000, "Reset To Default", "Values Set to Inital Default");
            //settings.setVehicle_maxSteerAngle = mf.vehicle.maxSteerAngle = 45; TODO
            settings.setVehicle_maxSteerAngle = 45;
            settings.setAS_countsPerDegree = 110;

            settings.setAS_ackerman = 100;

            settings.setAS_wasOffset = 3;

            settings.setAS_highSteerPWM = 180;
            settings.setAS_Kp = 50;
            settings.setAS_minSteerPWM = 25;

            settings.setArdSteer_setting0 = 56;
            settings.setArdSteer_setting1 = 0;
            settings.setArdMac_isDanfoss = false;

            settings.setArdSteer_maxPulseCounts = 3;

            settings.setVehicle_goalPointAcquireFactor = 0.85;
            settings.setVehicle_goalPointLookAheadHold = 3;
            settings.setVehicle_goalPointLookAheadMult = 1.5;

            settings.stanleyHeadingErrorGain = 1;
            settings.stanleyDistanceErrorGain = 1;
            settings.stanleyIntegralGainAB = 0;

            settings.purePursuitIntegralGainAB = 0;

            settings.setAS_sideHillComp = 0;

            settings.setAS_uTurnCompensation = 1;

            settings.setIMU_invertRoll = false;

            settings.setIMU_rollZero = 0;

            settings.setAS_minSteerSpeed = 0;
            settings.setAS_maxSteerSpeed = 15;
            settings.setAS_functionSpeedLimit = 12;
            settings.setDisplay_lightbarCmPerPixel = 5;
            settings.setDisplay_lineWidth = 2;
            settings.setAS_snapDistance = 20;
            settings.setAS_guidanceLookAheadTime = 1.5;
            settings.setAS_uTurnCompensation = 1;

            settings.setVehicle_isStanleyUsed = false;
            //mf.isStanleyUsed = false; TODO

            settings.setAS_isSteerInReverse = false;
            //mf.isSteerInReverse = false; TODO

            //save current vehicle
            //RegistrySettings.Save();

            /*TODO mf.vehicle = new CVehicle(mf);

            FormSteer_Load(this, e);

            toSend = true; counter = 6;


            pboxSendSteer.Visible = true;

            tabControl1.SelectTab(1);
            tabControl1.SelectTab(0);
            tabSteerSettings.SelectTab(1);
            tabSteerSettings.SelectTab(0);
            */
        }

        //region sensorsTab
        Item {
            visible: sensorsBtn.checked
            id: sensorWindowItem
            anchors.fill: parent
            ButtonGroup{
                buttons: sensorsBtnsRow.children
            }

            RowLayout{
                id: sensorsBtnsRow
                width: parent.width
                IconButtonColor{
                    id: cboxEncoder
                    icon.source: "../../images/Config/ConSt_TurnSensor.png"
                    checkable: true
                    buttonText: "Count Sensor"
                    Layout.alignment: Qt.AlignCenter
                    onClicked: unsaved.visible = true
                    onCheckedChanged: nudMaxCounts.visible = checked
                }
                IconButtonColor{
                    id: cboxPressureSensor
                    icon.source: "../../images/Config/ConSt_TurnSensorPressure.png"
                    checkable: true
                    buttonText: "Pressure Turn Sensor"
                    Layout.alignment: Qt.AlignCenter
                    onClicked: unsaved.visible = true
                }
                IconButtonColor{
                    id: cboxCurrentSensor
                    icon.source: "../../images/Config/ConSt_TurnSensorCurrent.png"
                    checkable: true
                    buttonText: "Current Turn Sensor"
                    Layout.alignment: Qt.AlignCenter
                    onClicked: unsaved.visible = true
                }
            }
            Text{
                //id: label61
                anchors.top: sensorsDisplayColumn.top
                anchors.right: sensorsDisplayColumn.left
                anchors.rightMargin: 10
                text: qsTr("Off at %")
                font.bold: true
                visible: cboxPressureSensor.checked || cboxCurrentSensor.checked
            }

            Column {
                id: sensorsDisplayColumn
                anchors.top: sensorsBtnsRow.bottom
                anchors.topMargin: 30 * theme.scaleHeight
                anchors.horizontalCenter: sensorsBtnsRow.horizontalCenter
                anchors.bottom: parent.bottom
                spacing: 5 * theme.scaleHeight
                width: childrenRect.width
                SpinBoxCustomized {
                    id: nudMaxCounts
                    text: qsTr("Counts")
                    implicitWidth: 100 * theme.scaleWidth
                    implicitHeight: 65 * theme.scaleHeight
                    from: 0
                    value: 0
                    visible: false
                    to: 255
                    decimals: 0
                    editable: true
                    onValueChanged: unsaved.visible = true
                }
                ProgressBar {
                    //id: pbarSensor
                    //just mirror width/height
                    width: 250 * theme.scaleWidth
                    height: 50 * theme.scaleWidth
                    visible: cboxPressureSensor.checked || cboxCurrentSensor.checked
                    value: aog.sensorData
                    Text {
                        //id: lblPercentFS
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.right
                        anchors.leftMargin: 5
                        text: (aog.sensorData < 0 ? "0" : aog.sensorData) + " %"
                        font.bold: true
                    }
                }
                SliderCustomized{
                    id: hsbarSensor
                    from: 0
                    to: 255
                    stepSize: 1
                    width: 250 * theme.scaleWidth
                    height: 50 * theme.scaleWidth
                    visible: cboxPressureSensor.checked || cboxCurrentSensor.checked
                    Text {
                        //id: lblhsbarSensor
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.right
                        anchors.leftMargin: 5
                        text: (hsbarSensor.value * 0.3921568627)  + " %"
                        font.bold: true
                    }
                }
            }
        }
        //endregion sensorsTab
        //
        //region configTab
        Item{
            anchors.fill: parent
            visible: configBtn.checked
            id: configWindow
            GridLayout{
                anchors.fill: parent
                rows: 4
                columns: 2
                flow: Grid.TopToBottom
                IconButtonColor{
                    id: cboxDanfoss
                    icon.source: "../../images/Config/ConST_Danfoss.png"
                    checkable: true
                    buttonText: "Danfoss"
                    Layout.alignment: Qt.AlignCenter
                    onClicked: unsaved.visible = true
                }
                IconButtonColor{
                    id: chkInvertWAS
                    icon.source: "../../images/Config/ConSt_InvertWAS.png"
                    checkable: true
                    Layout.alignment: Qt.AlignCenter
                    buttonText: "Invert WAS"
                    onClicked: unsaved.visible = true
                }
                IconButtonColor{
                    id: chkInvertSteer
                    icon.source: "../../images/Config/ConSt_InvertDirection.png"
                    checkable: true
                    buttonText: "Invert Motor Dir"
                    Layout.alignment: Qt.AlignCenter
                    onClicked: unsaved.visible = true
                }
                IconButtonColor{
                    id: chkSteerInvertRelays
                    icon.source: "../../images/Config/ConSt_InvertRelay.png"
                    checkable: true
                    buttonText: "Invert Relays"
                    Layout.alignment: Qt.AlignCenter
                    onClicked: unsaved.visible = true
                }
            }
            ColumnLayout{
                id: columnOfDropDown
                anchors.top: parent.top
                anchors.leftMargin: 10 * theme.scaleWidth
                anchors.rightMargin: 10 * theme.scaleWidth
                anchors.topMargin: 10 * theme.scaleHeight
                anchors.bottomMargin: 50 * theme.scaleHeight
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                height: parent.height /2
                ComboBoxCustomized {
                    id: cboxMotorDrive
                    editable: true
                    // Component.onCompleted: currentIndex = ((settings.setArdSteer_setting0 & 16) == 0) ? 1 : 0
                    model: ListModel {
                        id: modelmotorDriver
                        ListElement {text: "Cytron"}
                        ListElement {text: "IBT2"}
                    }
                    text: ("Motor Driver")
                    onActivated: unsaved.visible = true
                }
                ComboBoxCustomized {
                    id: cboxConv
                    editable: true
                    // Component.onCompleted: currentIndex = ((settings.setArdSteer_setting0 & 8) == 0) ? 1 : 0
                    model: ListModel {
                        id: a2Dmodel
                        ListElement {text: "Single"}
                        ListElement {text: "Differential"}
                    }
                    text: qsTr("A2D Converter")
                    onActivated: unsaved.visible = true
                }
                ComboBoxCustomized {
                    id: cboxXY
                    editable: true
                    // Component.onCompleted: currentIndex = ((settings.setArdSteer_setting1 & 8) == 0) ? 0 : 1
                    model: ListModel {
                        id: imuAxismodel
                        ListElement {text: "X"}
                        ListElement {text: "Y"}
                    }
                    text: qsTr("IMU X or Y Axis")
                    onActivated: unsaved.visible = true
                }

                ComboBoxCustomized {
                    id: cboxSteerEnable
                    /* Component.onCompleted: if((settings.setArdSteer_setting0 & 32) == 32)
                           currentIndex = 1
                           else if((settings.setArdSteer_setting0 & 64) == 64)
                           currentIndex = 2
                           else
                           currentIndex = 0*/
                    model: ListModel {
                        //   id: steerEnablemodel
                        ListElement {text: "None"}
                        ListElement {text: "Switch"}
                        ListElement {text: "Button"}
                    }
                    text: qsTr("Steer Enable")
                    onActivated: unsaved.visible = true
                    Text{
                        anchors.top: cboxSteerEnable.bottom
                        anchors.left: cboxSteerEnable.left
                        text: qsTr("Button- Push On, Push Off\nSwitch- Pushed On, Release Off")
                        font.pixelSize: 10
                    }
                }
            }
        }
        //endregion configTab
        //
        //region settingsTab
        Item{
            id: settingsWindow
            visible: settingsBtn.checked
            anchors.fill: parent
            Column {
                anchors.top: parent.top
                anchors.topMargin: 30 * theme.scaleHeight
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                spacing: 70 * theme.scaleHeight
                width: childrenRect.width
                SliderCustomized{
                    //Uturn Compensation
                    width: 250 * theme.scalewidth
                    centerTopText: qsTr("UTurn Compensation")
                    from: 2
                    to: 20
                    value: settings.setAS_uTurnCompensation * 10
                    onValueChanged: settings.setAS_uTurnCompensation = (value / 10)
                    leftText: value - 10
                }
                SliderCustomized {
                    //Sidehill deg
                    width: 250 * theme.scalewidth
                    centerTopText: qsTr("Sidehill Deg Turn per Deg of Roll")
                    from: 0.00
                    to:1.00
                    stepSize: .01
                    value: settings.setAS_sideHillComp
                    onValueChanged: settings.setAS_sideHillComp = value
                }
                Row{
                    spacing: 30 * theme.scaleWidth
                    IconButtonColor{
                        text: qsTr("Stanley/Pure")
                        isChecked: settings.setMenu_isPureOn
                        checkable: true
                        onCheckedChanged: settings.setMenu_isPureOn = checked
                        colorChecked: "white"
                        icon.source: "../../images/ModeStanley.png"
                        iconChecked: "../../images/ModePurePursuit.png"
                    }
                    IconButtonColor {
                        text: qsTr("Steer In Reverse?")
                        isChecked: settings.setAS_isSteerInReverse
                        checkable: true
                        onCheckedChanged: settings.setAS_isSteerInReverse = checked
                        icon.source: "../../images/Config/ConV_RevSteer.png"
                    }
                }
            }
        }
        //endregion settingsTab
        //region steerSettingsTab
        Item{
            id: steerSettingsWindow
            anchors.fill: parent
            visible: steerSettingsBtn.checked
            /*    Rectangle{
                        id: lightbarrect
                        anchors.left: parent.left
                        anchors.top: parent.top
                        height: 150
                        width: 400
                        anchors.margins: 20
                        color: "transparent"
                        Text{
                            id: lightbartitletxt
                            text: qsTr("LightBar - Distance per pixel")
                            anchors.top: parent.top
                            anchors.left: parent.left
                        }

                        Image {
                            id: lightbarimage
                            source: "../../images/Config/ConV_CmPixel.png"
                            anchors.left: parent.left
                            anchors.top: lightbartitletxt.bottom
                            anchors.bottom: parent.bottom
                            width: parent.width*.5
                            SpinBoxCM{
                                id: lightbarCmPerPixel
                                anchors.top: parent.top
                                anchors.topMargin: 25
                                height: 50
                                anchors.left: parent.right
                                anchors.leftMargin: 10
                                from: 0
                                to: 15
                                boundValue: settings.setDisplay_lightbarCmPerPixel
                                onValueModified: settings.setDisplay_lightbarCmPerPixel = value
                                editable: true
                                text: utils.cm_unit() + " " + qsTr("per pixel","As in units per pixel")
                            }
                        }
                    }
                    */
            IconButtonColor{
                anchors.right: parent.right
                anchors.top:parent.top
                anchors.topMargin: 20 * theme.scaleHeight
                anchors.bottomMargin: 20 * theme.scaleHeight
                anchors.leftMargin: 20 * theme.scaleWidth
                anchors.rightMargin: 20 * theme.scaleWidth
                icon.source: "../../images/AutoSteerOff.png"
                iconChecked: "../../images/AutoSteerOn.png"
                checkable: true
                color: "red"
                isChecked: settings.setAS_isAutoSteerAutoOn
                onCheckableChanged: settings.setAS_isAutoSteerAutoOn = checked
                text: qsTr("Steer Switch Control")
                font.pixelSize:15
                implicitWidth: 120 * theme.scaleWidth
                implicitHeight: 150 * theme.scaleHeight
            }

            //}
            //}
            Rectangle{
                id: linewidthrect
                anchors.left: parent.left
                anchors.top: nudgedistrect.bottom
                height: 150 * theme.scaleHeight
                width: 250 * theme.scaleWidth
                anchors.topMargin: 20 * theme.scaleHeight
                anchors.bottomMargin: 20 * theme.scaleHeight
                anchors.leftMargin: 20 * theme.scaleWidth
                anchors.rightMargin: 20 * theme.scaleWidth
                color: "transparent"
                TextLine{
                    id: linewidthtitletxt
                    text: qsTr("Line Width")
                    anchors.top: parent.top
                    anchors.left: parent.left
                }

                Image {
                    id: linewidthimage
                    source: "../../images/Config/ConV_LineWith.png"
                    anchors.left: parent.left
                    anchors.top: linewidthtitletxt.bottom
                    anchors.bottom: parent.bottom
                    width: parent.width*.5
                    SpinBoxCustomized{
                        id: linewidthSetting
                        anchors.top: parent.top
                        anchors.topMargin: 25
                        height: 50 * theme.scaleHeight
                        anchors.left: parent.right
                        anchors.leftMargin: 10 * theme.scaleWidth
                        from: 1
                        to: 8
                        boundValue: settings.setDisplay_lineWidth
                        onValueModified: settings.setDisplay_lineWidth = value
                        text: qsTr("pixels")
                        editable: true
                    }
                }
            }
            Rectangle{
                id: nudgedistrect
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                height: 100 * theme.scaleHeight
                width: 350 * theme.scaleWidth
                anchors.topMargin: 20 * theme.scaleHeight
                anchors.bottomMargin: 20 * theme.scaleHeight
                anchors.leftMargin: 20 * theme.scaleWidth
                anchors.rightMargin: 20 * theme.scaleWidth
                color: "transparent"
                TextLine{
                    id: nudgedisttitletxt
                    text: qsTr("Nudge Distance")
                    anchors.top: parent.top
                    anchors.left: parent.left
                }

                Image {
                    id: nudgedistimage
                    source: "../../images/Config/ConV_SnapDistance.png"
                    anchors.left: parent.left
                    anchors.top: nudgedisttitletxt.bottom
                    anchors.bottom: parent.bottom
                    width: parent.width*.5
                    SpinBoxCM{
                        id: snapDistance
                        anchors.top: parent.top
                        anchors.topMargin: 25
                        height: 50 * theme.scaleHeight
                        anchors.left: parent.right
                        anchors.leftMargin: 10 * theme.scaleWidth
                        from: 0
                        to: 1000
                        boundValue: settings.setAS_snapDistance
                        onValueModified: settings.setAS_snapDistance = value
                        editable: true
                        text: utils.cm_unit()
                    }
                }
            }
            Rectangle{
                id: lineacqLAheadrect
                anchors.left: linewidthrect.right
                anchors.verticalCenter: linewidthrect.verticalCenter
                anchors.topMargin: 50 * theme.scaleHeight
                anchors.bottomMargin: 50 * theme.scaleHeight
                anchors.leftMargin: 50 * theme.scaleWidth
                anchors.rightMargin: 50 * theme.scaleWidth
                height: 100 * theme.scaleHeight
                width: 350 * theme.scaleWidth
                color: "transparent"
                TextLine{
                    id: lineacqLAheadtitletxt
                    text: qsTr("Line Acquire Look Ahead")
                    anchors.top: parent.top
                    anchors.left: parent.left
                }

                Image {
                    id: lineacqLAheadimage
                    source: "../../images/Config/ConV_GuidanceLookAhead.png"
                    anchors.left: parent.left
                    anchors.top: lineacqLAheadtitletxt.bottom
                    anchors.bottom: parent.bottom
                    width: parent.width*.5
                    SpinBoxCustomized{
                        id: lineacqLAheadSetting
                        anchors.top: parent.top
                        anchors.topMargin: 25 * theme.scaleHeight
                        height: 50 * theme.scaleHeight
                        anchors.left: parent.right
                        anchors.leftMargin: 10 * theme.scaleWidth
                        from: .1
                        to: 10
                        boundValue: settings.setAS_guidanceLookAheadTime
                        onValueModified: settings.setAS_guidanceLookAheadTime = value
                        editable: true
                        text: qsTr("Seconds")
                        decimals: 1
                    }
                }
            }
            GridLayout{
                id: safety
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: linewidthrect.bottom
                anchors.topMargin: 10 * theme.scaleHeight
                anchors.bottomMargin: 10 * theme.scaleHeight
                anchors.leftMargin: 70 * theme.scaleWidth
                anchors.rightMargin: 70 * theme.scaleWidth
                flow: Grid.TopToBottom
                rows: 4
                columns: 4
                Text{ text: qsTr("Manual Turns Limit"); Layout.alignment: Qt.AlignCenter}
                Image{
                    source: "../../images/Config/con_VehicleFunctionSpeedLimit.png"
                    width: parent.width
                    height: 90 * theme.scaleHeight
                    Layout.alignment: Qt.AlignCenter
                    fillMode: Image.PreserveAspectFit
                }
                SpinBoxKM{
                    from: 0
                    to: 20
                    editable: true
                    boundValue: settings.setAS_functionSpeedLimit
                    onValueModified: settings.setAS_functionSpeedLimit = value
                    Layout.alignment: Qt.AlignCenter
                }
                Text{ text: qsTr(utils.speed_unit()); Layout.alignment: Qt.AlignCenter}
                Text{ text: qsTr("Min AutoSteer Speed"); Layout.alignment: Qt.AlignCenter}
                Image{
                    id: minAutoSteerImage
                    source: "../../images/Config/ConV_MinAutoSteer.png"
                    width: parent.width
                    height: 90 * theme.scaleHeight
                    Layout.alignment: Qt.AlignCenter
                    fillMode: Image.PreserveAspectFit
                }
                SpinBoxKM{
                    from: 0
                    to: 50
                    editable: true
                    boundValue: settings.setAS_minSteerSpeed
                    onValueModified: settings.setAS_minSteerSpeed = value
                    Layout.alignment: Qt.AlignCenter
                }
                Text{ text: qsTr(utils.speed_unit()); Layout.alignment: Qt.AlignCenter}
                Text{ text: qsTr("Max AutoSteer Speed"); Layout.alignment: Qt.AlignCenter}
                Image{
                    id: maxAutoSteerImage
                    source: "../../images/Config/ConV_MaxAutoSteer.png"
                    height: 90 * theme.scaleHeight
                    width: parent.width
                    Layout.alignment: Qt.AlignCenter
                    fillMode: Image.PreserveAspectFit
                }
                SpinBoxKM{
                    from: 0
                    to: 50
                    editable: true
                    boundValue: settings.setAS_maxSteerSpeed
                    onValueModified: settings.setAS_maxSteerSpeed = value
                    Layout.alignment: Qt.AlignCenter
                }
                Text{ text: qsTr(utils.speed_unit()); Layout.alignment: Qt.AlignCenter}
                Text{ text: qsTr("Max Turn Rate"); Layout.alignment: Qt.AlignCenter}
                Image{
                    source: "../../images/Config/ConV_MaxAngVel.png"
                    width: parent.width
                    height: 90 * theme.scaleHeight
                    Layout.alignment: Qt.AlignCenter
                    fillMode: Image.PreserveAspectFit
                }

                //The from and to values are deg/sec, but the final value output is in radians always
                SpinBoxCustomized {
                    Layout.alignment: Qt.AlignCenter
                    id: spinner
                    from: 5
                    to: 100
                    editable: true
                    value: utils.radians_to_deg(settings.setVehicle_maxAngularVelocity) // should be in radians!
                    onValueChanged: settings.setVehicle_maxAngularVelocity = utils.deg_to_radians(value)
                }
                Text{ text: qsTr("Degrees/sec"); Layout.alignment: Qt.AlignCenter}
            }
        }
        //endregion steerSettings
    }
    RowLayout{
        id: bottomRightButtons
        anchors.right: parent.right
        anchors.rightMargin: unsaved.width + 20
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10 * theme.scaleHeight
        height: wizard.height
        IconButtonText{
            id: wizard
            text: qsTr("Wizard")
            icon.source: "../../images/WizardWand.png"
            Layout.alignment: Qt.AlignCenter
            visible: false //TODO: because the wizard isn't implemented
        }
        IconButtonText{
            id: reset
            text: qsTr("Reset All To Defaults")
            icon.source: "../../images/Reset_Default.png"
            Layout.alignment: Qt.AlignCenter
            onClicked: {
                steerMessageDialog.close()
                steerMessageDialog.text = "Reset This Page to Defaults";
                steerMessageDialog.title = "Are you Sure";
                steerMessageDialog.buttons = MessageDialog.Yes | MessageDialog.No;

                // Connect new handlers for this instance
                steerMessageDialog.accepted.connect(() => {
                                                      settingsArea.reset_all();

                                                    });

                steerMessageDialog.visible = true;
            }
        }
        Text {
            text: qsTr("Send + Save")
            Layout.alignment: Qt.AlignRight
        }
        IconButton{
            id: send
            Layout.alignment: Qt.AlignLeft
            icon.source: "../../images/ToolAcceptChange.png"
            implicitWidth: 130
            onClicked: { settingsArea.save_settings() ; unsaved.visible = false }
        }
    }
    Image {
        id: unsaved
        width: 100 * theme.scaleWidth
        anchors.right: parent.right
        anchors.rightMargin: 10 * theme.scaleWidth
        anchors.verticalCenter: bottomRightButtons.verticalCenter
        visible: false
        source: "../../images/Config/ConSt_Mandatory.png"
    }

    MessageDialog{
        id: steerMessageDialog
        visible: false
    }
}
