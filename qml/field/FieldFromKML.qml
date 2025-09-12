// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// Menu when we load a field from KML
import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Dialogs

import ".."
import "../components"

Dialog{
    id: fieldFromKML
    height: 400  * theme.scaleHeight
    width:700  * theme.scaleWidth
    anchors.centerIn: parent
    visible: false
    function show(){
        parent.visible = true
    }
    TopLine{
        id: topLine
        titleText: qsTr("Load From KML")
    }

    Rectangle{
        id: textEntry
        width:parent.width*0.75
        height: 50  * theme.scaleHeight
        anchors.top:parent.top
        anchors.topMargin: 75
        anchors.horizontalCenter: parent.horizontalCenter
        color: aog.backgroundColor
        border.color: "darkgray"
        border.width: 1
        Text {
            id: newFieldLabel
            id: newFieldLabel
            anchors.left: parent.left
            anchors.bottom: parent.top
            font.bold: true
            font.pixelSize: 15
            text: qsTr("Enter Field Name")
        }
        TextField{
            id: newField
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: newFieldLabel.bottom
            height: 50
            selectByMouse: true
            placeholderText: focus || text ? "" : "New Field Name"
            onTextChanged: {
                for (var i=0; i < fieldInterface.field_list.length ; i++) {
                    if (text === fieldInterface.field_list[i].name) {
                        errorMessage.visible = true
                        break
                    } else
                        errorMessage.visible = false
                }
            }
        }
        Text {
            id: errorMessage
            anchors.top: newField.bottom
            anchors.left: newField.left
            color: "red"
            visible: false
            text: qsTr("This field exists already; please choose another name.")
        TextField{
            id: newField
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: newFieldLabel.bottom
            height: 50
            selectByMouse: true
            placeholderText: focus || text ? "" : "New Field Name"
            onTextChanged: {
                for (var i=0; i < fieldInterface.field_list.length ; i++) {
                    if (text === fieldInterface.field_list[i].name) {
                        errorMessage.visible = true
                        break
                    } else
                        errorMessage.visible = false
                }
            }
        }
        Text {
            id: errorMessage
            anchors.top: newField.bottom
            anchors.left: newField.left
            color: "red"
            visible: false
            text: qsTr("This field exists already; please choose another name.")
        }
    }
    Row{
        id: additives
        anchors.left: parent.left
        anchors.top: textEntry.bottom
        anchors.margins: 30
        spacing: 30
        IconButtonTransparent{
            objectName: "btnAddDate"
            id: marker
            icon.source: prefix + "/images/JobNameCalendar.png"
            Text{
                rightPadding: 10
                rightPadding: 10
                anchors.right: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: "+"
            }
            onClicked: {
                var date = new Date();
                var year = date.getFullYear();
                var month = String(date.getMonth() + 1).padStart(2, '0');
                var day = String(date.getDate()).padStart(2, '0');
                newField.text += " " + `${year}-${month}-${day}`
            }
            onClicked: {
                var date = new Date();
                var year = date.getFullYear();
                var month = String(date.getMonth() + 1).padStart(2, '0');
                var day = String(date.getDate()).padStart(2, '0');
                newField.text += " " + `${year}-${month}-${day}`
            }
        }
        IconButtonTransparent{
            objectName: "btnAddTime"
            icon.source: prefix + "/images/JobNameTime.png"
            Text{
                rightPadding: 10
                anchors.right: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: "+"
            }
            onClicked: {
                var date = new Date();
                var hours = String(date.getHours()).padStart(2, '0');
                var minutes = String(date.getMinutes()).padStart(2, '0');
                newField.text += " " + `${hours}-${minutes}`
            }
        }
    }
    IconButtonTransparent{
        objectName: "btnGetKML"
        icon.source: prefix + "/images/BoundaryLoadFromGE.png"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 20
        anchors.leftMargin: 30
        enabled: newField.text
        onClicked: fileDialog.open()
    }
    FileDialog {
        id: fileDialog
        onAccepted: {
            console.log("Selected file:", fileDialog.selectedFile)
        }
    }

    Row{
        id: saveClose
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        width: children.width
        height: children.height
        spacing: 10
        IconButtonTransparent{
            onClicked: fieldFromKML.visible = false
            icon.source: prefix + "/images/Cancel64.png"
        }
        IconButtonTransparent{
            objectName: "btnSave"
            icon.source: prefix + "/images/OK64.png"
            onClicked: {
                fieldFromKML.visible = false
                newField.text = ""
            }
        }
    }
}
