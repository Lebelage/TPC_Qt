import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "controls"

ApplicationWindow {
    id: root

    width: 1000
    height: 700
    visible: true
    title: "TPC"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Workspace {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        BottomHelperBar {
            Layout.fillWidth: true

            //connected: mainViewModel.connected
            endpoint: mainViewModel.endpoint
            connected: mainViewModel.connected

            onConnectRequested: function (endpoint) {
                mainViewModel.endpoint = endpoint
                mainViewModel.connectToServer()
            }

            onDisconnectRequested: {
                mainViewModel.disconnectFromServer()
            }
        }
    }
}