import QtQuick
import "components" as Comp

Grid{
    id: gridTurnBtns //lateral turn and manual Uturn
    spacing: 10
    rows: 2
    columns: 2
    flow: Grid.LeftToRight
    visible: aog.isBtnAutoSteerOn
    Comp.IconButtonTransparent{
        implicitHeight: 65 * theme.scaleHeight
        implicitWidth: 85 * theme.scaleWidth
        imageFillMode: Image.Stretch
        icon.source: "../../images/qtSpecific/z_TurnManualL.png"
        onClicked: {
            if (settings.setAS_functionSpeedLimit > aog.speedKph) {
                console.debug("limit ", settings.setAS_functionSpeedLimit, " speed ", aog.speedKph)
                aog.uturn(false)
            } else
                timedMessage.addMessage(2000,qsTr("Too Fast"), qsTr("Slow down below") + " " +
                                        utils.speed_to_unit_string(settings.setAS_functionSpeedLimit,1) + " " + utils.speed_unit())
        }

    }

    Comp.IconButtonTransparent{
        implicitHeight: 65 * theme.scaleHeight
        implicitWidth: 85 * theme.scaleWidth
        imageFillMode: Image.Stretch
        icon.source: "../../images/qtSpecific/z_TurnManualR.png"
        onClicked: {
            if (settings.setAS_functionSpeedLimit > aog.speedKph)
                aog.uturn(true)
            else
                timedMessage.addMessage(2000,qsTr("Too Fast"), qsTr("Slow down below") + " " +
                                        utils.speed_to_unit_string(settings.setAS_functionSpeedLimit,1) + " " + utils.speed_unit())
        }
    }
    Comp.IconButtonTransparent{
        implicitHeight: 65 * theme.scaleHeight
        implicitWidth: 85 * theme.scaleWidth
        imageFillMode: Image.Stretch
        icon.source: "../../images/qtSpecific/z_LateralManualL.png"
        onClicked: {
            if (settings.setAS_functionSpeedLimit > aog.speedKph)
                aog.lateral(false)
            else
                timedMessage.addMessage(2000,qsTr("Too Fast"), qsTr("Slow down below") + " " +
                                        aog.speed_to_unit_string(settings.setAS_functionSpeedLimit,1) + " " + aog.speed_unit())
        }
    }
    Comp.IconButtonTransparent{
        implicitHeight: 65 * theme.scaleHeight
        implicitWidth: 85 * theme.scaleWidth
        imageFillMode: Image.Stretch
        icon.source: "../../images/qtSpecific/z_LateralManualR.png"
        onClicked: {
            if (settings.setAS_functionSpeedLimit > aog.speedKph)
                aog.lateral(true)
            else
                timedMessage.addMessage(2000,qsTr("Too Fast"), qsTr("Slow down below") + " " +
                                        aog.speed_to_unit_string(settings.setAS_functionSpeedLimit,1) + " " + aog.speed_unit())
        }
    }
}
