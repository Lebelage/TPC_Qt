import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property var dataContext

    TableView {
        anchors.fill: parent
        
        model: root.dataContext ? root.dataContext.sensors_model : null

        delegate: Rectangle {
            implicitWidth: 140
            implicitHeight: 36
            border.color: "#3e4246"
            color: "#1e1f22"

            Text {
                anchors.centerIn: parent
                text: display
                color: "white"
            }
        }
    }
}