import QtQuick
import QtQuick.Layouts
import "components" as Comp

ColumnLayout {
    id: rightColumn //buttons

    visible: aog.isJobStarted

    onHeightChanged: {
        theme.btnSizes[0] = height / (children.length)
        theme.buttonSizesChanged()
    }
    onVisibleChanged: if(visible === false)
                          width = 0
                      else
                          width = children.width

    Comp.MainWindowBtns {
        property bool isContourLockedByUser //store if user locked
        id: btnContourLock
        isChecked: aog.btnIsContourLocked
        visible: btnContour.checked
        checkable: true
        icon.source: "../../images/ColorUnlocked.png"
        iconChecked: "../../images/ColorLocked.png"
        buttonText: "Lock"
        onClicked: {
            aog.btnContourLock()
            if (aog.btnIsContourLocked)
                isContourLockedByUser = true
        }
        Connections{
            target: aog
            function onBtnIsContourLockedChanged() {
                btnContourLock.checked = aog.btnIsContourLocked
                if(btnContourLock.isContourLockedByUser)
                    btnContourLock.isContourLockedByUser = false
            }
            function onIsBtnAutoSteerOnChanged() {
                if (!btnContourLock.isContourLockedByUser && btnContour.checked === true){
                    if(btnContourLock.checked !== aog.isBtnAutoSteerOn){
                        aog.btnContourLock()
                    }
                }
            }
        }
    }
    Comp.MainWindowBtns {
        id: btnContour
        isChecked: aog.isContourBtnOn //set value from backend
        checkable: true
        icon.source: "../../images/ContourOff.png"
        iconChecked: "../../images/ContourOn.png"
        buttonText: "Contour"
        onClicked: {
            aog.btnContour()
        }
        onCheckedChanged: { //gui logic
            btnTrackCycle.visible = !checked
            btnTrackCycleBk.visible = !checked
            btnContourPriority.visible = checked
        }
    }

    Comp.IconButton{
        id: btnTrackCycle
        icon.source: "../../images/ABLineCycle.png"
        Layout.alignment: Qt.AlignCenter
        implicitWidth: theme.buttonSize
        implicitHeight: theme.buttonSize
    }
    Comp.IconButton{
        id: btnTrackCycleBk
        icon.source: "../../images/ABLineCycleBk.png"
        Layout.alignment: Qt.AlignCenter
        implicitWidth: theme.buttonSize
        implicitHeight: theme.buttonSize
    }
    Comp.IconButton{
        id: btnAutoTrack
        checkable: true
        isChecked: aog.autoTrackBtnState
        icon.source: "../../images/AutoTrackOff.png"
        iconChecked: "../../images/AutoTrack.png"
        Layout.alignment: Qt.AlignCenter
        implicitWidth: theme.buttonSize
        implicitHeight: theme.buttonSize
        onCheckedChanged: checked ? aog.autoTrackBtnState = 1 : aog.autoTrackBtnState = 0
    }

    Comp.MainWindowBtns {
        id: btnSectionManual
        isChecked: aog.manualBtnState == 2
        checkable: true
        icon.source: "../../images/ManualOff.png"
        iconChecked: "../../images/ManualOn.png"
        buttonText: "Manual"
        onCheckedChanged: {
            if (checked) {
                btnSectionAuto.checked = false;
                sectionButtons.setAllSectionsToState(2 /*auto*/);
                aog.manualBtnState = 2 //btnStates::on

            } else {
                sectionButtons.setAllSectionsToState(0 /*off*/);
                aog.manualBtnState = 0
            }
        }
    }

    Comp.MainWindowBtns {
        id: btnSectionAuto
        isChecked: aog.autoBtnState == 1
        checkable: true
        icon.source: "../../images/SectionMasterOff.png"
        iconChecked: "../../images/SectionMasterOn.png"
        buttonText: "Auto"
        onCheckedChanged: {
            if (checked) {
                btnSectionManual.checked = false;
                sectionButtons.setAllSectionsToState(1 /*auto*/);
                aog.autoBtnState = 1 //btnStates::auto
            } else {
                sectionButtons.setAllSectionsToState(0 /*off*/);
                aog.autoBtnState = 0
            }
        }
    }
    Comp.MainWindowBtns {
        id: btnAutoYouTurn
        isChecked: aog.isYouTurnBtnOn
        checkable: true
        icon.source: "../../images/YouTurnNo.png"
        iconChecked: "../../images/YouTurn80.png"
        buttonText: "AutoUturn"
        visible: aog.isTrackOn
        enabled: aog.isBtnAutoSteerOn
        onClicked: aog.autoYouTurn()
    }
    Comp.MainWindowBtns {
        id: btnAutoSteer
        icon.source: "../../images/AutoSteerOff.png"
        iconChecked: "../../images/AutoSteerOn.png"
        checkable: true
        checked: aog.isBtnAutoSteerOn
        //enabled: aog.isTrackOn || aog.isContourBtnOn
        //Is remote activation of autosteer enabled? //todo. Eliminated in 6.3.3
        buttonText: (settings.setAS_isAutoSteerAutoOn === true ? "R" : "M")
        onClicked: {
            if (checked && ((trk.idx > -1) || btnContour.isChecked)) {
                console.debug("okay to turn on autosteer button.")
                aog.isBtnAutoSteerOn = true;
            } else {
                console.debug("keep autosteer button off.")
                checked = false;
                aog.isBtnAutoSteerOn = false;
            }
        }
        Connections {
            target: aog
            function onIsBtnAutoSteerOnChanged() {
                //TODO: use track interface in trk
                if (aog.isBtnAutoSteerOn && (trk.idx > -1)) {
                    btnAutoSteer.checked = true
                } else {
                    //default to turning everything off
                    btnAutoSteer.checked = false
                    aog.isBtnAutoSteerOn = false
                }
            }
            function onSpeedKphChanged() {
                if (btnAutoSteer.checked) {
                    if (aog.speedKph < settings.setAS_minSteerSpeed) {
                        aog.isBtnAutoSteerOn = false
                    } else if (aog.speedKph > settings.setAS_maxSteerSpeed) {
                        //timedMessage
                        aog.isBtnAutoSteerOn = false
                    }
                }
            }
        }
    }
}
