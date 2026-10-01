pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TPC.Native

import "../styles"

Window {
    id: root
    required property var dataContext

    width: 920
    height: 720
    minimumWidth: 680
    minimumHeight: 520
    title: "Field slice visualization"
    color: "#18191a"

    property var hoveredSample: ({})

    Connections {
        target: root.dataContext
        function onSliceChanged() { root.hoveredSample = ({}) }
    }

    function sampleAtItemPosition(x, y, width, height) {
        const imageAspect = root.dataContext.imageAspectRatio
        const itemAspect = width / Math.max(1, height)
        let drawWidth = width
        let drawHeight = height
        let offsetX = 0
        let offsetY = 0
        if (imageAspect > itemAspect) {
            drawHeight = width / imageAspect
            offsetY = (height - drawHeight) * 0.5
        } else {
            drawWidth = height * imageAspect
            offsetX = (width - drawWidth) * 0.5
        }
        return root.dataContext.sampleAt((x - offsetX) / drawWidth, (y - offsetY) / drawHeight)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 58
            radius: 8
            color: "#202226"
            border.color: "#3e4246"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Label { text: "Slice axis"; color: "#a9b7c6" }

                Repeater {
                    model: ["X", "Y", "Z"]
                    delegate: StyledButton {
                        required property string modelData
                        Layout.preferredWidth: 54
                        text: modelData
                        buttonColor: root.dataContext.axis === modelData ? "#3574a8" : "#2b2d30"
                        borderColor: root.dataContext.axis === modelData ? "#56a8f5" : "#4e5157"
                        onClicked: root.dataContext.setAxis(modelData)
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "Direct basis · double precision"
                    color: "#7f8b99"
                    font.pixelSize: 11
                }

                Label {
                    text: root.dataContext.positionLabel
                    color: "white"
                    font.family: "monospace"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#101113"
            border.color: "#3e4246"

            FieldSliceItem {
                id: sliceView
                anchors.fill: parent
                anchors.margins: 24
                model: root.dataContext
                onWidthChanged: root.dataContext.setViewportSize(width, height)
                onHeightChanged: root.dataContext.setViewportSize(width, height)
                Component.onCompleted: root.dataContext.setViewportSize(width, height)

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onPositionChanged: {
                        root.hoveredSample = root.sampleAtItemPosition(mouseX, mouseY, width, height)
                    }
                    onExited: root.hoveredSample = ({})
                }
            }

            Rectangle {
                visible: root.hoveredSample.value !== undefined
                x: 36
                y: 36
                width: sampleText.implicitWidth + 20
                height: sampleText.implicitHeight + 16
                radius: 6
                color: "#e6202226"
                border.color: "#59616b"

                Text {
                    id: sampleText
                    anchors.centerIn: parent
                    color: "white"
                    font.family: "monospace"
                    text: root.hoveredSample.value === undefined ? "" :
                          "x: " + Number(root.hoveredSample.x).toPrecision(7) +
                          "   y: " + Number(root.hoveredSample.y).toPrecision(7) +
                          "   z: " + Number(root.hoveredSample.z).toPrecision(7) + "\n" +
                          "Bx: " + Number(root.hoveredSample.bx).toPrecision(9) +
                          "   By: " + Number(root.hoveredSample.by).toPrecision(9) +
                          "   Bz: " + Number(root.hoveredSample.bz).toPrecision(9) + "\n" +
                          "|B|: " + Number(root.hoveredSample.value).toPrecision(10) + " G"
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: root.dataContext.loading
                visible: running
            }

            Label {
                anchors.centerIn: parent
                visible: !root.dataContext.available
                text: "Calculate the field to create slices"
                color: "#7f8b99"
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    text: Number(root.dataContext.minimumPosition).toPrecision(4)
                    color: "#a9b7c6"
                    font.family: "monospace"
                }

                Slider {
                    id: sliceSlider
                    Layout.fillWidth: true
                    from: root.dataContext.minimumPosition
                    to: root.dataContext.maximumPosition
                    stepSize: root.dataContext.positionStep
                    snapMode: Slider.SnapAlways
                    value: root.dataContext.slicePosition
                    enabled: root.dataContext.available && to > from
                    onMoved: root.dataContext.setSlicePosition(value)
                }

                Label {
                    text: Number(root.dataContext.maximumPosition).toPrecision(4) + " cm"
                    color: "#a9b7c6"
                    font.family: "monospace"
                }

                StyledTextField {
                    id: positionField
                    Layout.preferredWidth: 112
                    enabled: root.dataContext.available
                    text: Number(root.dataContext.slicePosition).toFixed(5)
                    placeholderText: "Position"
                    selectByMouse: true
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    defaultBorderColor: acceptableInput ? "#3e4246" : "#c75450"
                    activeBorderColor: acceptableInput ? "#3574f0" : "#e06c75"

                    validator: DoubleValidator {
                        bottom: root.dataContext.minimumPosition
                        top: root.dataContext.maximumPosition
                        decimals: 8
                        notation: DoubleValidator.StandardNotation
                        locale: "C"
                    }

                    function currentPositionText() {
                        return Number(root.dataContext.slicePosition).toFixed(5)
                    }

                    function commitPosition() {
                        if (!acceptableInput
                                || !root.dataContext.setSlicePosition(Number(text))) {
                            text = currentPositionText()
                        }
                    }

                    onAccepted: {
                        commitPosition()
                        focus = false
                    }
                    onEditingFinished: commitPosition()

                    Connections {
                        target: root.dataContext
                        function onSliceChanged() {
                            if (!positionField.activeFocus)
                                positionField.text = positionField.currentPositionText()
                        }
                    }
                }

                Label {
                    text: "cm"
                    color: "#a9b7c6"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: 170
                    implicitHeight: 14
                    radius: 3
                    clip: true
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#440154" }
                        GradientStop { position: 0.2; color: "#414487" }
                        GradientStop { position: 0.4; color: "#2a788e" }
                        GradientStop { position: 0.6; color: "#22a884" }
                        GradientStop { position: 0.8; color: "#7ad151" }
                        GradientStop { position: 1.0; color: "#fde725" }
                    }

                    Repeater {
                        model: Math.max(0, root.dataContext.inductionBandCount - 1)
                        Rectangle {
                            required property int index
                            x: (index + 1) * parent.width / root.dataContext.inductionBandCount
                            width: 1
                            height: parent.height
                            color: "#b5101113"
                        }
                    }
                }

                Label {
                    text: Number(root.dataContext.minimumValue).toPrecision(4) + " … " +
                          Number(root.dataContext.maximumValue).toPrecision(4) + " G  ·  Δ " +
                          Number(root.dataContext.inductionInterval).toPrecision(3) + " G"
                    color: "#a9b7c6"
                    font.family: "monospace"
                }
            }
        }
    }
}
