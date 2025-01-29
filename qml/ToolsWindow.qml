// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// The "Tools" button on main screen
import QtQuick 2.0
import QtQuick.Controls.Fusion

import ".."
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
                icon.source: "../../images/WizardWand.png"
                text: qsTr("Wizards")
                onClicked: wizardMenu.visible = !wizardMenu.visible
                visible: false //todo later
            }

            Comp.IconButtonTextBeside {
                id: charts
                icon.source: "../../images/Chart.png"
                text: qsTr("Charts")
                onClicked: chartsMenu.visible = !chartsMenu.visible
                visible: true
            }

            Comp.IconButtonTextBeside {
                id: smABCurve
                icon.source: "../../images/ABSmooth.png"
                text: qsTr("Smooth AB Curve")
                visible: settings.setFeature_isABSmoothOn
            }

            Comp.IconButtonTextBeside {
                id: delContourPaths
                icon.source: "../../images/TrashContourRef.png"
                width: 250
                height: 50
                text: qsTr("Delete Contour Paths")
                visible:settings.setFeature_isHideContourOn
            }

            Comp.IconButtonTextBeside {
                id: delAppliedArea
                icon.source: "../../images/TrashApplied.png"
                width: 250
                height: 50
                text: qsTr("Delete Applied Area")
                onClicked: aog.deleteAppliedArea()
            }

            Comp.IconButtonTextBeside {
                id: webcam
                icon.source: "../../images/Webcam.png"
                text: qsTr("WebCam")
                visible:settings.setFeature_isWebCamOn
            }

            Comp.IconButtonTextBeside {
                id: offsetFix
                icon.source: "../../images/YouTurnReverse.png" // this is horrible. This has nothing to do with YouTurnReverse.
                text: qsTr("Offset Fix")
                visible: settings.setFeature_isOffsetFixOn
            }
        }


    Rectangle { //this all needs to be done sometime
        id: wizardMenu
        width: 270 * theme.scaleWidth
        height: mainWindow.height
        visible: false
        color: "black"
        border.color: "lime"
        anchors.left: toolsMenu.right



        Grid {
            id: grid2
            height: childrenRect.height
            width: childrenRect.width
            anchors.left: parent.left
            anchors.leftMargin: 5
            anchors.top: parent.top
            anchors.topMargin: 5
            spacing: 10
            flow: Grid.TopToBottom
            rows: 1
            columns: 1

            Comp.IconButtonTextBeside{
                id: steerWiz
                width: 250
                height: 50
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
                icon.source: "../../images/AutoSteerOn.png"
                //onClicked: chartsMenu.visible = !chartsMenu.visible, toolsMenu.visible = false, steerCharta.visible = true
                onClicked:  steerCharta.visible = true
                visible: true
            }
            Comp.IconButtonTextBeside{
                id: headingChart
                text: qsTr("Heading Chart")
            }
            Comp.IconButtonTextBeside{
                id: xteChart
                text: qsTr("XTE Chart")
            }
            Comp.IconButtonTextBeside{
                id: rollChart
                text: qsTr("Roll Chart")
            }
        }
    }
    Wiz.ChartSteer{
    id: steerCharta
    height: 300  * theme.scaleHeight
    width: 500  * theme.scaleWidth
    }
}
