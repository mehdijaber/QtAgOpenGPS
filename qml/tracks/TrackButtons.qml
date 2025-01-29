// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// The panel of buttons where track editing(new, go to select which track, line and ref nudge, etc)
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Fusion
import "../components"
//import "../"


TimedRectangle {
    id: trackButtons
    width: 550
    color: "white"
    height: 75
    RowLayout{
        anchors.fill: parent
        anchors.margins: 5
        IconButtonTransparent{
            id: marker
            icon.source: "../../images/ABSnapNudgeMenuRef.png"
            Layout.alignment: Qt.AlignCenter
            onClicked: refNudge.show()
        }
        IconButtonColor{
            icon.source: "../../images/AutoSteerSnapToPivot.png"
            implicitWidth: marker.width
            implicitHeight: marker.width
            Layout.alignment: Qt.AlignCenter
        }
        IconButtonTransparent{
            icon.source: "../../images/SwitchOff.png"
            Layout.alignment: Qt.AlignCenter
            onClicked: {
                tracksInterface.select(-1);
            }
        }
        IconButtonTransparent{
            icon.source: "../../images/ABTracks.png"
            Layout.alignment: Qt.AlignCenter
            onClicked: trackList.show()
        }
        IconButtonTransparent{
            icon.source: "../../images/AddNew.png"
            Layout.alignment: Qt.AlignCenter
            onClicked: trackNew.show()
        }
        IconButtonTransparent{
            icon.source: "../../images/ABDraw.png"
            Layout.alignment: Qt.AlignCenter
            onClicked: lineDrawer.show()
        }
        IconButtonTransparent{
            icon.source: "../../images/ABSnapNudgeMenu.png"
            Layout.alignment: Qt.AlignCenter
            onClicked: lineNudge.show()
        }
    }
}
