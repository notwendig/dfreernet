import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    objectName: "attendantWindow"
    visible: true
    width: 500
    height: 390
    minimumWidth: 440
    minimumHeight: 370
    title: tr("R-Net Control")
    color: "#f2f5fa"
    font.pixelSize: 14
    palette.button: "#e5edf8"
    palette.buttonText: "#18304d"
    palette.highlight: "#245eaa"
    function tr(value) { return qsTranslate("RNet", value) }
    onClosing: (close) => { close.accepted = controller.requestClose() }
    Connections { target: controller; function onCloseReady() { window.close() } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label { text: window.tr("R-Net Control"); font.pixelSize: 24; font.bold: true; color: "#18304d"; Layout.fillWidth: true }
            Label { text: controller.canInterface + " · " + controller.canCount; color: "#526b87" }
        }
        Label {
            objectName: "controlStatus"
            text: controller.statusText
            font.bold: true
            color: controller.attendantControl ? "#13744b" : "#526b87"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle { radius: 14; color: "white"; border.color: "#d9e2ef" }
            RowLayout {
                anchors.fill: parent
                spacing: 20
                JoystickPad {
                    id: pad
                    enabled: controller.controlsEnabled && controller.attendantControl
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 180
                    xValue: controller.inputX / 100
                    yValue: controller.inputY / 100
                    onInputRequested: (x, y) => controller.setAxes(x, y)
                }
                ColumnLayout {
                    spacing: 10
                    Layout.preferredWidth: 166
                    Button {
                        id: takeover
                        objectName: "takeControlButton"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 62
                        enabled: controller.controlsEnabled
                        text: controller.attendantControl ? window.tr("Return to passenger") : window.tr("Take control")
                        contentItem: Text { text: takeover.text; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: takeover.enabled ? "white" : "#64748b"; font.bold: true }
                        background: Rectangle { radius: 10; color: !takeover.enabled ? "#e1e7ef" : takeover.pressed ? "#183f78" : "#245eaa" }
                        onClicked: controller.toggleDrivingControl()
                    }
                    Button {
                        objectName: "profileButton"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        enabled: controller.controlsEnabled && (!controller.canTxAllowed || controller.attendantControl)
                        text: window.tr("Profile %1").arg(controller.profile) + "\n" + window.tr("Mode %1").arg(controller.modeNumber)
                        onClicked: controller.nextProfile()
                    }
                    Button {
                        objectName: "speedButton"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        enabled: controller.controlsEnabled && (!controller.canTxAllowed || controller.attendantControl)
                        text: window.tr("Speed %1%").arg(controller.inputLimit)
                        onClicked: controller.nextSpeed()
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: "X " + controller.outputX + "   Y " + controller.outputY; font.bold: true; color: "#18304d"; Layout.fillWidth: true }
            Label { text: controller.canTxAllowed ? window.tr("Sent: %1").arg(controller.canTxCount) : window.tr("Simulation"); color: "#526b87" }
        }
        Label { visible: controller.detail.length > 0; text: controller.detail; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#875611"; Layout.fillWidth: true }
    }
}
