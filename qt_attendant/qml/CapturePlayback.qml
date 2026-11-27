import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    objectName: "capturePlayback"
    visible: true
    width: 560
    height: 550
    minimumWidth: 460
    minimumHeight: 510
    title: tr("Offline capture playback")
    color: "#f2f5fa"
    palette.button: "#e5edf8"
    palette.buttonText: "#18304d"
    palette.highlight: "#245eaa"
    font.pixelSize: 14
    property real position: 0
    property int index: -1
    property real lastTick: 0
    property real rate: 1
    readonly property var current: index >= 0 ? captureEvents[index] : null
    function tr(value) { return qsTranslate("RNet", value) }
    function timeLabel(seconds) {
        return Math.floor(seconds / 60) + ":" + Math.floor(seconds % 60).toString().padStart(2, "0")
    }
    function seek(t) {
        position = Math.max(0, Math.min(captureDuration, t))
        let lo = 0, hi = captureEvents.length
        while (lo < hi) {
            const mid = (lo + hi) >> 1
            if (captureEvents[mid].time <= position) lo = mid + 1
            else hi = mid
        }
        index = lo - 1
        lastTick = Date.now()
    }
    function togglePlaying() {
        if (clock.running) clock.stop()
        else {
            if (position >= captureDuration) seek(0)
            lastTick = Date.now()
            clock.start()
        }
    }
    function jump(direction) {
        clock.stop()
        let i = index + direction
        while (i >= 0 && i < captureEvents.length) {
            const event = captureEvents[i]
            if (event.phase !== "Joystick" && Math.abs(event.time - position) > 0.001) {
                seek(event.time)
                return
            }
            i += direction
        }
        seek(direction < 0 ? 0 : captureDuration)
    }
    Component.onCompleted: seek(0)
    Shortcut { sequence: "Space"; onActivated: root.togglePlaying() }
    Shortcut { sequence: "Left"; onActivated: { clock.stop(); root.seek(root.position - 5) } }
    Shortcut { sequence: "Right"; onActivated: { clock.stop(); root.seek(root.position + 5) } }
    Shortcut { sequence: "Home"; onActivated: { clock.stop(); root.seek(0) } }
    Shortcut { sequence: "End"; onActivated: { clock.stop(); root.seek(captureDuration) } }
    Timer {
        id: clock
        interval: 30
        repeat: true
        onTriggered: {
            const next = root.position + (Date.now() - root.lastTick) / 1000 * root.rate
            root.seek(next)
            if (root.position >= captureDuration) stop()
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Label { text: root.tr("Capture analysis"); color: "#18304d"; font.pixelSize: 24; font.bold: true; Layout.fillWidth: true }
            Label { text: root.tr("Offline"); color: "#526b87" }
        }
        Frame {
            Layout.fillWidth: true
            background: Rectangle { color: "white"; radius: 12; border.color: "#d9e2ef" }
            ColumnLayout {
                anchors.fill: parent
                spacing: 10
                Label {
                    text: root.tr(root.current?.phase ?? "Start")
                    color: "#245eaa"; font.bold: true; font.pixelSize: 18
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                }
                RowLayout {
                    spacing: 18
                    JoystickPad {
                        enabled: false
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 150
                        xValue: (root.current?.x ?? 0) / 100
                        yValue: (root.current?.y ?? 0) / 100
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Label { text: root.tr("Confirmed control slot"); color: "#526b87"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        Label { text: root.current?.owner >= 0 ? root.current.owner : "—"; color: "#18304d"; font.pixelSize: 32; font.bold: true }
                        Label { text: "X " + (root.current?.x ?? 0) + "    Y " + (root.current?.y ?? 0); color: "#18304d"; font.bold: true }
                        Label { text: root.tr("Mode/profile byte") + "  " + (root.current?.selection ?? "—"); color: "#526b87" }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: root.tr("Enumerated identity"); color: "#526b87" }
                    Label { text: root.current?.serial || "—"; color: "#18304d"; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; elide: Text.ElideMiddle }
                }
            }
        }
        Frame {
            Layout.fillWidth: true
            background: Rectangle { color: "white"; radius: 12; border.color: "#d9e2ef" }
            ColumnLayout {
                anchors.fill: parent
                spacing: 10
                RowLayout {
                    Label { text: root.timeLabel(root.position); color: "#18304d"; font.bold: true; Layout.fillWidth: true }
                    Label { text: root.timeLabel(captureDuration); color: "#526b87" }
                }
                Slider {
                    objectName: "playbackTimeline"
                    Layout.fillWidth: true
                    to: captureDuration
                    value: root.position
                    onPressedChanged: if (pressed) clock.stop()
                    onMoved: root.seek(value)
                    Accessible.name: root.tr("Position")
                }
                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        objectName: "previousEvent"
                        text: root.tr("Previous")
                        enabled: root.position > 0
                        onClicked: root.jump(-1)
                        ToolTip.text: root.tr("Previous protocol event")
                        ToolTip.visible: hovered
                    }
                    Button {
                        objectName: "playPause"
                        text: clock.running ? root.tr("Pause") : root.tr("Play")
                        highlighted: true
                        Layout.fillWidth: true
                        onClicked: root.togglePlaying()
                    }
                    Button {
                        objectName: "nextEvent"
                        text: root.tr("Next")
                        enabled: root.position < captureDuration
                        onClicked: root.jump(1)
                        ToolTip.text: root.tr("Next protocol event")
                        ToolTip.visible: hovered
                    }
                    ComboBox {
                        objectName: "playbackSpeed"
                        model: ["0.5×", "1×", "2×", "4×"]
                        currentIndex: 1
                        implicitWidth: 78
                        onActivated: { root.rate = [0.5, 1, 2, 4][currentIndex]; root.lastTick = Date.now() }
                        Accessible.name: root.tr("Playback speed")
                    }
                }
            }
        }
        Label { text: root.tr("Space: play / pause · Arrow keys: ±5 seconds"); color: "#526b87"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Item { Layout.fillHeight: true }
    }
}
