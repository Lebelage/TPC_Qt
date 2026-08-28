import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property bool connected: false
    property string endpoint: ""

    signal connectRequested(string endpoint)
    signal disconnectRequested()

    implicitHeight: 56
    color: "#2b2d30"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Label {
            text: root.connected
                ? "Connected"
                : "Disconnected"

            color: "white"
        }

        Item {
            Layout.fillWidth: true
        }

        TextField {
            id: endpointField

            text: root.endpoint
            placeholderText: "opc.tcp://127.0.0.1:1234"

            Layout.preferredWidth: 320
        }

        Button {
            text: root.connected
                ? "Disconnect"
                : "Connect"

            onClicked: {
                if (root.connected) {
                    root.disconnectRequested()
                } else {
                    root.connectRequested(endpointField.text)
                }
            }
        }
    }
}