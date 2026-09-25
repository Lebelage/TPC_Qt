import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../styles"

Rectangle {
    id: root
    implicitHeight: 56
    color: "#2b2d30"

    required property var dataContext

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 12

        Label {
            text: root.dataContext.isConnected ? "Connected" : "Disconnected"
            color: root.dataContext.isConnected ? "#499c54" : "#e05555"
        }

        Item {
            Layout.fillWidth: true
        }

        StyledButton {
            Layout.preferredWidth: 100
            cornerRadius: 8
            text: root.dataContext.isConnected ? "Disconnect" : "Connect"
            onClicked: {
                if (root.dataContext.isConnected)
                    root.dataContext.disconnectFromTpc()
                else
                    root.dataContext.connectToTpc()
            }
        }

        StyledButton {
            Layout.preferredWidth: 100
            cornerRadius: 8
            text: root.dataContext.switchTabButtonName

            onClicked: {
                root.dataContext.toggleTab()
            }
        }
    }
}
