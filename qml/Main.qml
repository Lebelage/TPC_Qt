import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "controls"
import "styles"

ApplicationWindow {
    id: root
    width: 1000
    height: 700
    visible: true
    title: "TPC Controller"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StackLayout {
            id: mainViewStack
            Layout.fillWidth: true
            Layout.fillHeight: true

            currentIndex: mainViewModel.current_tab_index

            Workspace {
                dataContext: mainViewModel.workspace
            }
            
            Settings {
                dataContext: mainViewModel.settings
            }
        }

        BottomBar {
            Layout.fillWidth: true
            dataContext: mainViewModel.bottomBar
        }
    }
}