import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var dataContext

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 36
            color: "#18191a"
            border.color: "#3e4246"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 12

                Text {
                    Layout.preferredWidth: 220
                    text: "Sensor Name"
                    color: "#a9b7c6"
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    text: "Value"
                    color: "#a9b7c6"
                    font.bold: true
                }

                Text {
                    Layout.preferredWidth: 100
                    text: "Actions"
                    color: "#a9b7c6"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            model: root.dataContext ? root.dataContext.sensors_model : null

            delegate: Rectangle {
                width: listView.width
                height: 48
                border.color: "#3e4246"
                color: "#1e1f22"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 12

                    Text {
                        Layout.preferredWidth: 220
                        text: sensorName
                        color: "white"
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: sensorValue
                        color: "#499c54"
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 56
            color: "#1e1f22"
            border.color: "#3e4246"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10

                Item { Layout.fillWidth: true }

                Button {
                    text: "Refresh Data"
                    onClicked: {
                        root.dataContext.try_get_frame_command();
                    }
                }
            }
        }
    }
}