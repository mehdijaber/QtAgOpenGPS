// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// The "Tools" button on main screen
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Settings
import AOG
import Interface
import "components" as Comp
import "wizards" as Wiz

//Item{
//    id: toolsWindowItem
//    visible: false
//    height: mainWindow.height
    Drawer {
        id: toolsMenu
        width: 270 * theme.scaleWidth
        height: mainWindow.height
        modal: true
//        onVisibleChanged: if (visible === false){
//                             toolsWindowItem.visible = false
//                          }

        contentItem: Rectangle{
            id: toolsMenuContent
            anchors.fill: parent
            height: toolsMenu.height
            color: aog.blackDayWhiteNight
        }

        Comp.ScrollViewExpandableColumn {
            id: toolsGrid
            anchors.fill: parent

            Comp.IconButtonTextBeside {
                id: wizards
                icon.source: prefix + "/images/WizardWand.png"
                text: qsTr("Wizards")
                onClicked: wizardMenu.visible = !wizardMenu.visible
                visible: true //todo later
            }

            Comp.IconButtonTextBeside {
                id: charts
                icon.source: prefix + "/images/Chart.png"
                text: qsTr("Charts")
                onClicked: chartsMenu.visible = !chartsMenu.visible
                visible: true
            }

            Comp.IconButtonTextBeside {
                id: smABCurve
                icon.source: prefix + "/images/ABSmooth.png"
                text: qsTr("Smooth AB Curve")
                visible: Settings.feature_isABSmoothOn
            }

            Comp.IconButtonTextBeside {
                id: delContourPaths
                icon.source: prefix + "/images/TrashContourRef.png"
                text: qsTr("Delete Contour Paths")
                visible:Settings.feature_isHideContourOn
            }

            // Comp.IconButtonTextBeside {
            //     id: webcam
            //     icon.source: prefix + "/images/Webcam.png"
            //     text: qsTr("WebCam")
            //     visible:Settings.feature_isWebCamOn
            //     onClicked: cam1.visible = !cam1.visible, toolsMenu.visible = false
            // }

            Comp.IconButtonTextBeside {
                id: offsetFix
                icon.source: prefix + "/images/YouTurnReverse.png" // this is horrible. This has nothing to do with YouTurnReverse.
                text: qsTr("Offset Fix")
                visible: Settings.feature_isOffsetFixOn
            }
        }


        Drawer {
            id: wizardMenu
            width: 270 * theme.scaleWidth
            height: mainWindow.height
            modal: true

            contentItem: Rectangle{
                id: wizardMenuContent
                anchors.fill: parent
                height: wizardMenuMenu.height
                color: aog.blackDayWhiteNight
            }

            Grid {
                id: grid2
                height: childrenRect.height
                width: childrenRect.width
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 5
                spacing: 10
                flow: Grid.TopToBottom
                rows: 2
                columns: 1

            Comp.IconButtonTextBeside{
                id: wasWiz
                text: qsTr("Was Wizard")
                onClicked: { wizardMenu.visible = false ; wasWizard.show()}
            }

            Comp.IconButtonTextBeside{
                id: steerWiz
                text: qsTr("Steer Wizard")
            }
        }
    }
    Drawer {
        id: chartsMenu
        width: 270 * theme.scaleWidth
        height: mainWindow.height
        modal: true
//        onVisibleChanged: if (visible === false){
//                             toolsWindowItem.visible = false
//                          }

        contentItem: Rectangle{
            id: chartsMenuContent
            anchors.fill: parent
            height: chartsMenu.height
            color: aog.blackDayWhiteNight
        }

        Comp.ScrollViewExpandableColumn {
            id: chartsGrid
            anchors.fill: parent

            Comp.IconButtonTextBeside{
                id: steerChart
                text: qsTr("Steer Chart")
                icon.source: prefix + "/images/AutoSteerOn.png"
                onClicked: chartsMenu.visible = !chartsMenu.visible, toolsMenu.visible = false, steerCharta.show()
                visible: true
            }
            Comp.IconButtonTextBeside{
                id: headingChart
                text: qsTr("Heading Chart")
                onClicked: chartsMenu.visible = !chartsMenu.visible, toolsMenu.visible = false, headingCharta.show()
                icon.source: prefix + "/images/Config/ConS_SourcesHeading.png"

            }
            Comp.IconButtonTextBeside{
                id: xteChart
                text: qsTr("XTE Chart")
                icon.source: prefix + "/images/AutoManualIsAuto.png"
                onClicked: chartsMenu.visible = !chartsMenu.visible, toolsMenu.visible = false, xteCharta.show()
                visible: true
            }
            Comp.IconButtonTextBeside{
                id: rollChart
                text: qsTr("Roll Chart")
                icon.source: prefix + "/images/Config/ConDa_InvertRoll.png"
            }
        }
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
    }

    Wiz.ChartSteer{
    id: headingCharta
    height: 300  * theme.scaleHeight
    width: 400  * theme.scaleWidth
    xval1: aog.gpsHeading
    xval2: Number(aog.imuCorrected)
    axismin: -10
    axismax: 10
    lineName1:"Fix2fix"
    lineName2:"IMU"
    chartName: qsTr("Heading Chart")
    }
    //xval1 = (glm.toDegrees(mf.gpsHeading)).ToString("N1", CultureInfo.InvariantCulture);
    //xval2 = (glm.toDegrees(mf.imuCorrected)).ToString("N1", CultureInfo.InvariantCulture);

    Wiz.Camera{
    id: cam1
    height: 300  * theme.scaleHeight
    width: 400  * theme.scaleWidth
    }
}
