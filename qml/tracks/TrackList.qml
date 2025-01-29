// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// The window where we select which track we want
import QtQuick
import QtQuick.Controls
//import QtQuick.Controls.Styles 1.4
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQml.Models
import "../components"
import ".."

MoveablePopup {
    //AOGInterface {
    //    id: aog //temporary
    //}
    id: trackPickerDialog

    width: 600
    height: 450

    modal: true

    //signal updateTracks()
    //signal deleteLine(int lineno)
    //signal changeName(int lineno)
    //signal addLine(string name, double easting, double northing, double heading)
    //signal setA(bool start_cancel); //true to mark an A point, false to cancel new point

	function show() {
		trackPickerDialog.visible = true
	}

    onVisibleChanged:  {
        //when we show or hide the dialog, ask the main
        //program to update our lines list in the
        //AOGInterface object
        //linesInterface.abLine_updateLines()
        trackView.currentIndex = trk.idx
        //preselect first AB line if none was in use before
        //to make it faster for user
        if (trackView.currentIndex < 0)
            if (trk.model.count > 0)
                trackView.currentIndex = 0
    }

    Rectangle{
        anchors.fill: parent
        border.width: 1
        border.color: aog.blackDayWhiteNight
        color: aog.backgroundColor
        TopLine{
            id: topLine
            titleText: "Tracks"
        }
        ColumnLayout{
            id: leftColumn
            anchors.top: topLine.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.rightMargin: 1
            anchors.bottomMargin: 1
            width: childrenRect.width
            IconButtonTransparent{
				icon.source: "../../images/Trash.png"
				onClicked: {
                    if (trackView.currentIndex > -1) {
                        if (aog.currentTrack === trackView.currentIndex)
						aog.currentTrack = -1
                        linesInterface.abLine_deleteLine(trackView.currentIndex)
                        trackView.currentIndex = -1
					}
				}
            }
            IconButtonTransparent{
                icon.source: "../../images/FileEditName.png"
                onClicked: {
                    if (trackView.currentIndex > -1) {
                        editLineName.set_name(linesInterface.abLinesList[trackView.currentIndex].name)
                        editLineName.visible = true
                    }
                }
            }
            IconButtonTransparent{
                objectName: "btnLineCopy"
                icon.source: "../../images/FileCopy.png"
                onClicked: {
                    if(trackView.currentIndex > -1) {
                        copyLineName.set_name("Copy of " + linesInterface.abLinesList[trackView.currentIndex].name)
                        copyLineName.visible = true
                    }
                }
            }
            IconButtonTransparent{
                objectName: "btnLineSwapPoints"
                icon.source: "../../images/ABSwapPoints.png"
                onClicked: {
                    if(trackView.currentIndex > -1)
                        linesInterface.abLine_swapHeading(trackView.currentIndex);
                }
            }
            IconButtonTransparent{
				icon.source: "../../images/Cancel64.png"
				onClicked: {
					trackPickerDialog.visible = false
				}
			}
		}
		ColumnLayout{
			id: rightColumn
			anchors.top: topLine.bottom
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.rightMargin: 1
            anchors.bottomMargin: 1
            width: childrenRect.width
            IconButtonTransparent{ //not sure what this does in aog--doesn't work on wine
                icon.source: "../../images/UpArrow64.png"
            }
            IconButtonTransparent{
                icon.source: "../../images/DnArrow64.png"
            }
            IconButtonTransparent{
                icon.source: "../../images/ABLinesHideShow.png"
            }
			IconButtonTransparent{
				icon.source: "../../images/AddNew.png"
				onClicked: {
                    trackNewButtons.show()
					trackListDialog.visible = false
				}
			}
            IconButtonTransparent{
                objectName: "btnLineExit" //this is not cancel, rather, save and exit
                icon.source: "../../images/OK64.png"
                onClicked: {
                    trackPickerDialog.visible = false
                    if (trackView.selected > -1 && trackView.trackVisible) {
                        console.debug("Activating track ", trackView.selected)
                        tracksInterface.select(trackView.selected)
                    }
                    trackPickerDialog.visible = false
                }
            }
        }
        Rectangle{
            id: listrect
            anchors.left: leftColumn.right
            anchors.top:topLine.bottom
            anchors.right: rightColumn.left
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 0
            anchors.margins: 10

            //ListModel { //this will be populated by the backend cpp code
              //  id: trackModel
                //objectName: "trackModel"
			//}

            //See MockTrack.qml for static test model

            TracksListView {
                id: trackView
                anchors.fill: parent
                model: trk.model
                //property int currentIndex: -1

                clip: true
            }
        }
    }
}

