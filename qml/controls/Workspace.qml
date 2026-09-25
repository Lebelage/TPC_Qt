pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../styles"

Item {
    id: root
    required property var dataContext
    signal visualizationRequested()

    readonly property int sensorColumnWidth: 140
    readonly property int numericColumnWidth: 110

    component HeaderCell: Text {
        Layout.minimumWidth: root.numericColumnWidth
        Layout.preferredWidth: root.numericColumnWidth
        Layout.maximumWidth: root.numericColumnWidth
        color: "#a9b7c6"
        font.bold: true
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
    }

    component NumericCell: Text {
        required property real numericValue
        property color valueColor: "#a9b7c6"

        Layout.minimumWidth: root.numericColumnWidth
        Layout.preferredWidth: root.numericColumnWidth
        Layout.maximumWidth: root.numericColumnWidth
        text: Number(numericValue).toFixed(3)
        color: valueColor
        font.family: "monospace"
        horizontalAlignment: Text.AlignHCenter
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 58
            color: "#18191a"
            border.color: "#3e4246"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 12

                Text {
                    Layout.minimumWidth: root.sensorColumnWidth
                    Layout.preferredWidth: root.sensorColumnWidth
                    Layout.maximumWidth: root.sensorColumnWidth
                    text: "Sensor"
                    color: "#a9b7c6"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }

                ColumnLayout {
                    Layout.minimumWidth: root.numericColumnWidth * 3
                    Layout.preferredWidth: root.numericColumnWidth * 3
                    Layout.maximumWidth: root.numericColumnWidth * 3
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: "Components"
                        color: "#7f8b99"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        HeaderCell { text: "R" }
                        HeaderCell { text: "F" }
                        HeaderCell { text: "Z" }
                    }
                }

                ColumnLayout {
                    Layout.minimumWidth: root.numericColumnWidth * 3
                    Layout.preferredWidth: root.numericColumnWidth * 3
                    Layout.maximumWidth: root.numericColumnWidth * 3
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: "Coordinates"
                        color: "#7f8b99"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        HeaderCell { text: "X" }
                        HeaderCell { text: "Y" }
                        HeaderCell { text: "Z" }
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            model: root.dataContext ? root.dataContext.sensorsModel : null

            delegate: Rectangle {
                id: sensorRow
                required property int index
                required property string sensorName
                required property double componentR
                required property double componentF
                required property double componentZ
                required property double coordinateX
                required property double coordinateY
                required property double coordinateZ

                width: ListView.view.width
                height: 48
                border.color: "#3e4246"
                color: sensorRow.index % 2 === 0 ? "#1e1f22" : "#202226"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 12

                    Text {
                        Layout.minimumWidth: root.sensorColumnWidth
                        Layout.preferredWidth: root.sensorColumnWidth
                        Layout.maximumWidth: root.sensorColumnWidth
                        text: sensorRow.sensorName
                        color: "white"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.minimumWidth: root.numericColumnWidth * 3
                        Layout.preferredWidth: root.numericColumnWidth * 3
                        Layout.maximumWidth: root.numericColumnWidth * 3
                        spacing: 0

                        NumericCell {
                            numericValue: sensorRow.componentR
                            valueColor: "#56a8f5"
                        }
                        NumericCell {
                            numericValue: sensorRow.componentF
                            valueColor: "#56a8f5"
                        }
                        NumericCell {
                            numericValue: sensorRow.componentZ
                            valueColor: "#56a8f5"
                        }
                    }

                    RowLayout {
                        Layout.minimumWidth: root.numericColumnWidth * 3
                        Layout.preferredWidth: root.numericColumnWidth * 3
                        Layout.maximumWidth: root.numericColumnWidth * 3
                        spacing: 0

                        NumericCell { numericValue: sensorRow.coordinateX }
                        NumericCell { numericValue: sensorRow.coordinateY }
                        NumericCell { numericValue: sensorRow.coordinateZ }
                    }

                    Item { Layout.fillWidth: true }
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

                Item {
                    Layout.fillWidth: true
                }

                StyledButton {
                    text: "Visualize field"
                    cornerRadius: 8
                    enabled: root.dataContext && root.dataContext.fieldCalculated
                    onClicked: root.visualizationRequested()
                }

                StyledButton {
                    text: "Save VTK"
                    cornerRadius: 8
                    enabled: root.dataContext && root.dataContext.fieldCalculated
                    onClicked: {
                        root.dataContext.saveFieldAsVtk();
                    }
                }

                StyledButton {
                    text: "Calculate field"
                    cornerRadius: 8
                    onClicked: {
                        root.dataContext.calculateField();
                    }
                }
            }
        }
    }
}
