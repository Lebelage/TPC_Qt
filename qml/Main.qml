import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "controls"

ApplicationWindow {
    id: root
    required property var mainViewModel

    width: 1000
    height: 700
    visible: true
    title: "TPC Controller"
    color: "#18191a"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StackLayout {
            id: mainViewStack
            Layout.fillWidth: true
            Layout.fillHeight: true

            currentIndex: root.mainViewModel.currentTabIndex

            Workspace {
                dataContext: root.mainViewModel.workspace
                onVisualizationRequested: fieldVisualizationWindow.show()
            }
            
            Settings {
                dataContext: root.mainViewModel.settings
            }
        }

        BottomBar {
            Layout.fillWidth: true
            dataContext: root.mainViewModel.bottomBar
        }
    }

    FieldVisualization {
        id: fieldVisualizationWindow
        dataContext: root.mainViewModel.fieldVisualization
    }
}
