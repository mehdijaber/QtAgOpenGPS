import QtQuick
import QtQuick.Layouts
import "components" as Comp

    Rectangle{
        id: topLine
        color: aog.backgroundColor
        height: 50 *theme.scaleHeight
        visible: true

        Comp.IconButtonTransparent {
            id: btnfileMenu
            height: parent.height
            width: 75 * theme.scaleWidth
            icon.source: "../../images/fileMenu.png"
            onClicked: hamburgerMenu.visible = true
        }

        Text{
            anchors.top:parent.top
            anchors.left: parent.left
            anchors.leftMargin: leftColumn.width+20
            text: (aog.fixQuality === 0 ? "Invalid":
                   aog.fixQuality ===1 ? "GPS Single":
                   aog.fixQuality ===2 ? "DGPS":
                   aog.fixQuality ===3 ? "PPS":
                   aog.fixQuality ===4 ? "RTK Fix":
                   aog.fixQuality ===5 ? "RTK Float":
                   aog.fixQuality ===6 ? "Estimate":
                   aog.fixQuality ===7 ? "Man IP":
                   aog.fixQuality ===8 ? "Sim":
                   "Invalid") + ": Age: "+ Math.round(aog.age * 10)/ 10

            font.pixelSize: 20
            anchors.bottom: parent.verticalCenter
        }

        //        Text {
        //            anchors.top: parent.top
        //            anchors.left: parent.left
        //            anchors.leftMargin: 120
        //            text: qsTr("Field: "+ (aog.isJobStarted ? settings.setF_CurrentDir: "None"))
        //            anchors.bottom: parent.verticalCenter
        //            font.bold: true
        //            font.pixelSize: 15
        //        }
        Text {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.leftMargin: 150
            text: qsTr("ac")
            anchors.top: parent.verticalCenter
            font.bold: true
            font.pixelSize: 15
        }
        Text {
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("ab")
            font.bold: true
            font.pixelSize: 15
        }
        Row{
            id: topRowWindow
            width: childrenRect.width
            height: parent.height
            anchors.top: parent.top
            anchors.right: parent.right
            spacing: 5 * theme.scaleWidth
            Comp.IconButton {
                id: btnFieldInfo
                icon.source: "../../images/FieldStats.png"
                Layout.alignment: Qt.AlignCenter
                implicitWidth: theme.buttonSize
                height:parent.height
                visible: aog.isJobStarted
                onClicked: {
                    fieldData.visible = !fieldData.visible
                    gpsData.visible = false
                    blockageData.visible = false
                }
            }
            Comp.IconButtonColor{
                id: rtkStatus
                icon.source: "../../images/GPSQuality.png"
                implicitWidth: 75 * theme.scaleWidth
                implicitHeight: parent.height
                color: "yellow"
                onClicked: {
                    gpsData.visible = !gpsData.visible
                    fieldData.visible = false
                    blockageData.visible = false
                }
                Connections{
                    target: aog
                    function onFixQualityChanged() {
                        if(aog.fixQuality == 4) rtkStatus.color = "green"
                        else if(aog.fixQuality == 5) rtkStatus.color = "orange"
                        else if(aog.fixQuality == 2) rtkStatus.color = "yellow"
                        else rtkStatus.color = "red"
                    }
                }

            }
            Comp.IconButton {
                id: btnBlockageInfo
                icon.source: "../../images/Blockage.png"
                Layout.alignment: Qt.AlignCenter
                implicitWidth: theme.buttonSize
                height:parent.height
                visible: aog.blockageConnected
                onClicked: {
                    blockageData.visible = !blockageData.visible
                    gpsData.visible = false
                    fieldData.visible = false
                }
            }


            Text{
                id: speed
                anchors.verticalCenter: parent.verticalCenter
                width: 75 * theme.scaleWidth
                height:parent.height
                text: utils.speed_to_unit_string(aog.speedKph, 1)
                font.bold: true
                font.pixelSize: 35
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            Comp.IconButtonTransparent{
                height: parent.height
                icon.source: "../../images/WindowMinimize.png"
                width: 75 * theme.scaleWidth
                onClicked: mainWindow.showMinimized()
            }
            Comp.IconButtonTransparent{
                id: btnMaximize
                height: parent.height
                icon.source: "../../images/WindowMaximize.png"
                width: 75 * theme.scaleWidth
                onClicked: {
                    console.debug("Visibility is " + mainWindow.visibility)
                    if (mainWindow.visibility == Window.FullScreen){
                        mainWindow.showNormal()
                    }else{
                        settings.setWindow_Size = ((mainWindow.width).toString() + ", "+  (mainWindow.height).toString())
                        mainWindow.showFullScreen()
                    }
                }
            }
            Comp.IconButtonTransparent{
                height: parent.height
                width: 75 * theme.scaleWidth
                icon.source: "../../images/WindowClose.png"
                onClicked: {
                    mainWindow.save_everything()
                    mainWindow.close()
                }
            }
        }
    }
