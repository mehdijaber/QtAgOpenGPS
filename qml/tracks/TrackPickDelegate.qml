import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

RadioDelegate {
    id: trackDelegate

    required property int index
    required property string name
    required property bool isVisible
    required property int mode

    property int scrollbar_width: 10


    property double controlWidth: width
    width: parent.width - scrollbar_width

    contentItem: RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right - trackDelegate.scrollbar_width

        Image {
            id: trackType

            source: (trackDelegate.mode === 2) ? "../../images/TrackLine.png" :
                    (trackDelegate.mode === 4) ? "../../images/TrackCurve.png" :
                    (trackDelegate.mode === 64) ? "../../images/TrackPivot.png" :
                                       "../../images/HelpSmall.png"
        }

            Text {
                id: trackname
                Layout.fillWidth: true

                //rightPadding: trackDelegate.indicator.width + trackDelegate.spacing
                text: trackDelegate.name
                font: trackDelegate.font
                opacity: enabled ? 1.0 : 0.3
                color: aog.textColor
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

        Button {
            id: isTrackVisible
            background: Rectangle {
                implicitWidth: trackname.height
                implicitHeight: trackname.height
                color: trackDelegate.isVisible ? "green" : "red"
            }
            onClicked: {
                tracksInterface.setVisible(trackDelegate.index, !trackDelegate.isVisible)
            }
        }
    }

    indicator: Rectangle {
    }

    background: Rectangle {
        visible: trackDelegate.down || trackDelegate.highlighted || trackDelegate.checked
        color: trackDelegate.checked ? aog.borderColor : aog.backgroundColor
    }
}

