import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

        TextField {
            text: root.dataContext.endpoint
            onTextEdited: root.dataContext.endpoint = text
            Layout.preferredWidth: 300
        }

        Button {
            width: 100
            text: root.dataContext.isConnected ? "Disconnect" : "Connect"
            onClicked: {
                if (root.dataContext.isConnected)
                    root.dataContext.disconnection_command()
                else
                    root.dataContext.connection_command()
            }
        }

        Button {
            width: 100
            text: root.dataContext.switch_tab_button_name

            onClicked: {
                root.dataContext.switch_tab_command()
            }
        }
    }
}