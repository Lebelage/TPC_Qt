import QtQuick
import QtQuick.Controls

TextField {
    id: control

    property int cornerRadius: 6
    property color baseColor: "#1e1f22"
    property color focusColor: "#2b2d30"
    property color defaultBorderColor: "#3e4246"
    property color activeBorderColor: "#3574f0"
    property color customTextColor: "#dfdfe0"
    property color placeholderColor: "#6c707e" // Переименовано во избежание конфликта

    font.pixelSize: 13
    color: control.customTextColor
    placeholderTextColor: control.placeholderColor
    selectionColor: "#264f78"
    selectedTextColor: "#ffffff"
    verticalAlignment: TextInput.AlignVCenter

    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 6

    background: Rectangle {
        implicitWidth: 160
        implicitHeight: 32
        radius: control.cornerRadius
        color: control.activeFocus ? control.focusColor : control.baseColor
        border.color: control.activeFocus ? control.activeBorderColor : control.defaultBorderColor
        border.width: control.activeFocus ? 2 : 1

        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }
}
