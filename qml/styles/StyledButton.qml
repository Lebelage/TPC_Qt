import QtQuick
import QtQuick.Controls

Button {
    id: control

    property int cornerRadius: 6
    property color buttonColor: "#2b2d30"
    property color hoverColor: "#393b40"
    property color pressColor: "#1e1f22"
    property color borderColor: "#4e5157"

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? "white" : "#6c707e"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: 90
        implicitHeight: 32
        radius: control.cornerRadius
        color: control.down ? control.pressColor
            : control.hovered ? control.hoverColor
                : control.buttonColor
        border.color: control.borderColor
        border.width: 1
    }
}
