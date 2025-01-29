import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components" as Comp

RowLayout{
    property bool hydLiftIsOn: btnHydLift.isOn
    id:bottomButtons
    visible: aog.isJobStarted && leftColumn.visible

    onWidthChanged: {
        theme.btnSizes[1] = width / (children.length)
        theme.buttonSizesChanged()
    }
    onVisibleChanged: {
        if (visible === false)
            height = 0
        else
            height = children.height

    }
    ComboBox {
        id: cbYouSkipNumber
        editable: true
        Layout.alignment: Qt.AlignCenter
        implicitWidth: theme.buttonSize
        implicitHeight: theme.buttonSize
        model: ListModel {
            id: model
            ListElement {text: "1"}
            ListElement {text: "2"}
            ListElement {text: "3"}
            ListElement {text: "4"}
            ListElement {text: "5"}
            ListElement {text: "6"}
            ListElement {text: "7"}
            ListElement {text: "8"}
            ListElement {text: "9"}
            ListElement {text: "10"}
        }
        onAccepted: {
            if (cbYouSkipNumber.find(currentText) === -1){
                model.append({text: editText})
                curentIndex = cbYouSkipNumber.find(editText)
            }
        }
    }
    Comp.MainWindowBtns {
        id: btnYouSkip // the "Fancy Skip" button
        isChecked: false
        checkable: true
        icon.source: "../../images/YouSkipOff.png"
        iconChecked: "../../images/YouSkipOn.png"
        buttonText: "YouSkips"
    }
    Comp.MainWindowBtns { //reset trailing tool to straight back
        id: btnResetTool
        icon.source: "../../images/ResetTool.png"
        buttonText: "Reset Tool"
        onClicked: aog.btnResetTool()
        visible: settings.setTool_isToolTrailing === true //hide if front or rear 3 pt
    }
    Comp.MainWindowBtns {
        id: btnSectionMapping
        icon.source: "../../images/SectionMapping.png"
        onClicked: cpSectionColor.open()
    }
    Comp.MainWindowBtns {
        id: btnTramLines
        icon.source: "../../images/TramLines.png"
        buttonText: "Tram Lines"
        Layout.alignment: Qt.AlignCenter
        implicitWidth: theme.buttonSize
        implicitHeight: theme.buttonSize
    }
    Comp.MainWindowBtns {
        property bool isOn: false
        id: btnHydLift
        isChecked: isOn
        checkable: true
        disabled: btnHeadland.checked
        visible: utils.isTrue(settings.setArdMac_isHydEnabled) && btnHeadland.visible
        icon.source: "../../images/HydraulicLiftOff.png"
        iconChecked: "../../images/HydraulicLiftOn.png"
        buttonText: "HydLift"
        onClicked: {
            isOn = !isOn
            aog.isHydLiftOn(isOn)
        }
    }
    Comp.MainWindowBtns {
        id: btnHeadland
        isChecked: aog.isHeadlandOn
        checkable: true
        icon.source: "../../images/HeadlandOff.png"
        iconChecked: "../../images/HeadlandOn.png"
        buttonText: "Headland"
        onClicked: aog.btnHeadland()
    }
    Comp.MainWindowBtns {
        id: btnFlag
        objectName: "btnFlag"
        isChecked: false
        icon.source: "../../images/FlagRed.png"
        onPressAndHold: {
            if (contextFlag.visible) {
                contextFlag.visible = false;
            } else {
                contextFlag.visible = true;
            }
        }
        buttonText: "Flag"
    }

    Comp.MainWindowBtns {
        id: btnTrack
        icon.source: "../../images/TrackOff.png"
        iconChecked: "../../images/TrackOn.png"
        buttonText: "Track"
        onClicked: trackButtons.visible = !trackButtons.visible
    }

}
