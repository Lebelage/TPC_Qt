import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../styles"

Item {
    id: root

    required property var dataContext

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 0

        Item { Layout.fillHeight: true }

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(796, Math.max(0, root.width - 48))
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                GroupBox {
                    id: geometrySettings
                    title: "Geometry"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 390

                    topPadding: 34
                    bottomPadding: 18
                    leftPadding: 18
                    rightPadding: 18

                    label: Label {
                        x: geometrySettings.leftPadding
                        width: geometrySettings.availableWidth
                        text: geometrySettings.title
                        color: "#dfdfe0"
                        font.bold: true
                        font.pixelSize: 14
                    }

                    background: Rectangle {
                        y: geometrySettings.topPadding / 2
                        width: geometrySettings.width
                        height: geometrySettings.height - geometrySettings.topPadding / 2
                        color: "#1e1f22"
                        border.color: "#4e5157"
                        border.width: 1
                        radius: 8
                    }

                    GridLayout {
                        width: parent.width
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 12

                        Label {
                            text: "TPC length, cm"
                            color: "#a9b7c6"
                            Layout.alignment: Qt.AlignVCenter
                        }
                        StyledTextField {
                            id: lengthField
                            Layout.fillWidth: true
                            placeholderText: "Enter length"
                            text: root.dataContext ? root.dataContext.tpcLength.toString() : ""
                            inputMethodHints: Qt.ImhFormattedNumbersOnly

                            validator: DoubleValidator {
                                bottom: 0.001
                                top: 10000.0
                                decimals: 4
                                notation: DoubleValidator.StandardNotation
                                locale: "C"
                            }

                            onTextEdited: {
                                if (acceptableInput && root.dataContext)
                                    root.dataContext.tpcLength = Number(text)
                            }
                        }

                        Label {
                            text: "TPC radius, cm"
                            color: "#a9b7c6"
                            Layout.alignment: Qt.AlignVCenter
                        }
                        StyledTextField {
                            id: radiusField
                            Layout.fillWidth: true
                            placeholderText: "Enter radius"
                            text: root.dataContext ? root.dataContext.tpcRadius.toString() : ""
                            inputMethodHints: Qt.ImhFormattedNumbersOnly

                            validator: DoubleValidator {
                                bottom: 0.001
                                top: 10000.0
                                decimals: 4
                                notation: DoubleValidator.StandardNotation
                                locale: "C"
                            }

                            onTextEdited: {
                                if (acceptableInput && root.dataContext)
                                    root.dataContext.tpcRadius = Number(text)
                            }
                        }
                    }
                }

                GroupBox {
                    id: connectionSettings
                    title: "Connection"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 390

                    topPadding: 34
                    bottomPadding: 18
                    leftPadding: 18
                    rightPadding: 18

                    label: Label {
                        x: connectionSettings.leftPadding
                        width: connectionSettings.availableWidth
                        text: connectionSettings.title
                        color: "#dfdfe0"
                        font.bold: true
                        font.pixelSize: 14
                    }

                    background: Rectangle {
                        y: connectionSettings.topPadding / 2
                        width: connectionSettings.width
                        height: connectionSettings.height - connectionSettings.topPadding / 2
                        color: "#1e1f22"
                        border.color: "#4e5157"
                        border.width: 1
                        radius: 8
                    }

                    GridLayout {
                        width: parent.width
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 12

                        Label {
                            text: "Polling interval, ms"
                            color: "#a9b7c6"
                            Layout.alignment: Qt.AlignVCenter
                        }
                        StyledTextField {
                            id: pollingIntervalField
                            Layout.fillWidth: true
                            placeholderText: "Enter interval"
                            text: root.dataContext ? root.dataContext.pollingInterval.toString() : ""
                            inputMethodHints: Qt.ImhDigitsOnly

                            validator: IntValidator { bottom: 1; top: 60000 }

                            onTextEdited: {
                                if (acceptableInput && root.dataContext)
                                    root.dataContext.pollingInterval = parseInt(text, 10)
                            }
                        }

                        Label {
                            text: "URL"
                            color: "#a9b7c6"
                            Layout.alignment: Qt.AlignVCenter
                        }
                        StyledTextField {
                            id: urlField
                            Layout.fillWidth: true
                            placeholderText: "opc.tcp://127.0.0.1:1234"
                            text: root.dataContext ? root.dataContext.endpoint : ""

                            onTextEdited: {
                                if (root.dataContext)
                                    root.dataContext.endpoint = text
                            }
                        }
                    }
                }
            }

            GroupBox {
                id: gridSettings
                title: "3D grid"
                Layout.fillWidth: true

                topPadding: 34
                bottomPadding: 18
                leftPadding: 18
                rightPadding: 18

                label: Label {
                    x: gridSettings.leftPadding
                    width: gridSettings.availableWidth
                    text: gridSettings.title
                    color: "#dfdfe0"
                    font.bold: true
                    font.pixelSize: 14
                }

                background: Rectangle {
                    y: gridSettings.topPadding / 2
                    width: gridSettings.width
                    height: gridSettings.height - gridSettings.topPadding / 2
                    color: "#1e1f22"
                    border.color: "#4e5157"
                    border.width: 1
                    radius: 8
                }

                ColumnLayout {
                    width: parent.width
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: "Number of calculation cells along each axis"
                        color: "#7f8b99"
                        font.pixelSize: 12
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 16

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Label { text: "Nx · X axis"; color: "#a9b7c6" }
                            StyledTextField {
                                id: gridNxField
                                Layout.fillWidth: true
                                placeholderText: "Nx"
                                text: root.dataContext ? root.dataContext.gridNx.toString() : ""
                                horizontalAlignment: TextInput.AlignHCenter
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 1; top: 1000 }

                                onTextEdited: {
                                    if (acceptableInput && root.dataContext)
                                        root.dataContext.gridNx = parseInt(text, 10)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Label { text: "Ny · Y axis"; color: "#a9b7c6" }
                            StyledTextField {
                                id: gridNyField
                                Layout.fillWidth: true
                                placeholderText: "Ny"
                                text: root.dataContext ? root.dataContext.gridNy.toString() : ""
                                horizontalAlignment: TextInput.AlignHCenter
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 1; top: 1000 }

                                onTextEdited: {
                                    if (acceptableInput && root.dataContext)
                                        root.dataContext.gridNy = parseInt(text, 10)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Label { text: "Nz · Z axis"; color: "#a9b7c6" }
                            StyledTextField {
                                id: gridNzField
                                Layout.fillWidth: true
                                placeholderText: "Nz"
                                text: root.dataContext ? root.dataContext.gridNz.toString() : ""
                                horizontalAlignment: TextInput.AlignHCenter
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 1; top: 1000 }

                                onTextEdited: {
                                    if (acceptableInput && root.dataContext)
                                        root.dataContext.gridNz = parseInt(text, 10)
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                StyledButton {
                    text: "Load"
                    cornerRadius: 8

                    onClicked: {
                        if (root.dataContext)
                            root.dataContext.loadSettings()
                    }
                }

                StyledButton {
                    text: "Apply"
                    cornerRadius: 8
                    buttonColor: "#3574f0"
                    hoverColor: "#467ff2"
                    pressColor: "#2f65d0"
                    borderColor: "#3574f0"

                    onClicked: {
                        if (root.dataContext)
                            root.dataContext.applySettings()
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
