import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../styles"

Item {
    id: root

    required property var dataContext

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillHeight: true
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 16

            RowLayout {
                spacing: 16

                GroupBox {
                    id: tpc_settings
                    title: "TPC settings"
                    Layout.preferredWidth: 380

                    topPadding: 32
                    bottomPadding: 16
                    leftPadding: 16
                    rightPadding: 16

                    label: Label {
                        x: tpc_settings.leftPadding
                        width: tpc_settings.availableWidth
                        text: tpc_settings.title
                        color: "white"
                        font.bold: true
                        font.pixelSize: 13
                    }

                    background: Rectangle {
                        y: tpc_settings.topPadding / 2
                        width: tpc_settings.width
                        height: tpc_settings.height - tpc_settings.topPadding / 2
                        border.color: "white"
                        color: "transparent"
                        border.width: 2
                        radius: 6
                    }

                    GridLayout {
                        width: parent.width
                        columns: 2
                        columnSpacing: 14
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

                            validator: DoubleValidator {
                                bottom: 0.001
                                top: 10000.0
                                decimals: 4
                                notation: DoubleValidator.StandardNotation
                                locale: "C"
                            }

                            inputMethodHints: Qt.ImhFormattedNumbersOnly

                            onTextEdited: {
                                if (acceptableInput && root.dataContext) {
                                    root.dataContext.tpcLength = Number(text)
                                }
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

                            validator: DoubleValidator {
                                bottom: 0.001
                                top: 10000.0
                                decimals: 4
                                notation: DoubleValidator.StandardNotation
                                locale: "C"
                            }

                            inputMethodHints: Qt.ImhFormattedNumbersOnly

                            onTextEdited: {
                                if (acceptableInput && root.dataContext) {
                                    root.dataContext.tpcRadius = Number(text)
                                }
                            }
                        }
                    }
                }

                GroupBox {
                    id: connection_settings
                    title: "Connection settings"
                    Layout.preferredWidth: 380

                    topPadding: 32
                    bottomPadding: 16
                    leftPadding: 16
                    rightPadding: 16

                    label: Label {
                        x: connection_settings.leftPadding
                        width: connection_settings.availableWidth
                        text: connection_settings.title
                        color: "white"
                        font.bold: true
                        font.pixelSize: 13
                    }

                    background: Rectangle {
                        y: connection_settings.topPadding / 2
                        width: connection_settings.width
                        height: connection_settings.height - connection_settings.topPadding / 2
                        border.color: "white"
                        color: "transparent"
                        border.width: 2
                        radius: 6
                    }

                    GridLayout {
                        width: parent.width
                        columns: 2
                        columnSpacing: 14
                        rowSpacing: 12

                        Label {
                            text: "Polling interval, ms"
                            color: "#a9b7c6"
                            Layout.alignment: Qt.AlignVCenter
                        }
                        StyledTextField {
                            id: polling_interval
                            Layout.fillWidth: true
                            placeholderText: "Enter interval"
                            text: root.dataContext ? root.dataContext.pollingInterval.toString() : ""

                            validator: IntValidator {
                                bottom: 0
                                top: 60000
                            }
                            inputMethodHints: Qt.ImhDigitsOnly

                            onTextEdited: {
                                if (acceptableInput && root.dataContext) {
                                    root.dataContext.pollingInterval = parseInt(text, 10) || 0
                                }
                            }
                        }

                        Label {
                            text: "URL"
                            color: "#a9b7c6"
                            Layout.alignment: Qt.AlignVCenter
                        }

                        StyledTextField {
                            id: url
                            Layout.fillWidth: true
                            placeholderText: "example: opc.tcp://127.0.0.1:1234"
                            text: root.dataContext ? root.dataContext.endpoint : ""

                            onTextEdited: {
                                if (root.dataContext) {
                                    root.dataContext.endpoint = text
                                }
                            }
                        }
                    }
                }
            }

            StyledButton {
                text: "Apply"
                cornerRadius: 8
                Layout.alignment: Qt.AlignRight

                onClicked: {
                    if (root.dataContext) {
                        root.dataContext.apply_settings_command()
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
