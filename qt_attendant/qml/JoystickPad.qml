import QtQuick

Item {
    id: pad
    objectName: "joystickPad"
    property real xValue: 0
    property real yValue: 0
    property bool inputActive: Window.active
    onInputActiveChanged: area.cancelInput()
    signal inputRequested(real horizontal, real vertical)
    implicitWidth: 300
    implicitHeight: 300
    readonly property real radius: Math.min(width, height) * 0.38
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(pad.width, pad.height) - 8
        height: width
        radius: width / 2
        color: pad.enabled ? "#edf3fc" : "#f1f3f6"
        border.color: "#c7d5e8"
        border.width: 2
        Rectangle { anchors.centerIn: parent; width: 1; height: parent.height * 0.82; color: "#d0dced" }
        Rectangle { anchors.centerIn: parent; height: 1; width: parent.width * 0.82; color: "#d0dced" }
        Rectangle { anchors.centerIn: parent; width: parent.width * 0.5; height: width; radius: width/2; color: "transparent"; border.color: "#d0dced" }
    }
    Rectangle {
        objectName: "joystickKnob"
        width: 64; height: 64; radius: 32
        x: pad.width/2 - width/2 + pad.xValue * pad.radius
        y: pad.height/2 - height/2 - pad.yValue * pad.radius
        color: pad.enabled ? "#245eaa" : "#8794a6"
        border.width: 6; border.color: "#ffffff"
    }
    Timer {
        interval: 50
        repeat: true
        // Background UI heartbeats carry neutral only. If the UI stalls, the
        // independent controller watchdog still expires as before.
        running: pad.enabled || !pad.inputActive
        onTriggered: {
            if (pad.inputActive && point.pressed && area.pointerArmed) area.updateInput()
            else pad.inputRequested(0, 0)
        }
    }
    MultiPointTouchArea {
        id: area
        objectName: "joystickTouchArea"
        anchors.fill: parent
        mouseEnabled: true
        minimumTouchPoints: 1
        maximumTouchPoints: 1
        touchPoints: [TouchPoint { id: point }]
        property bool pointerArmed: false
        // Output is already neutral in C++. Clear the cached UI input after
        // bindings settle, without reentering the enabled binding itself.
        onEnabledChanged: if (!enabled) {
            pointerArmed = false
            Qt.callLater(() => pad.inputRequested(0, 0))
        }
        function cancelInput() {
            pointerArmed = false
            // Disable already centers in C++. Avoid reentering the enabled
            // binding when Qt cancels an active touch during suspension.
            if (pad.enabled) pad.inputRequested(0, 0)
        }
        function updateInput() {
            if (!pad.inputActive || !pointerArmed) return
            let x = (point.x - pad.width/2) / pad.radius
            let y = (pad.height/2 - point.y) / pad.radius
            const length = Math.sqrt(x*x+y*y)
            if (length > 1) { x /= length; y /= length }
            pad.inputRequested(x*100, y*100)
        }
        onGestureStarted: (gesture) => gesture.grab()
        onPressed: {
            pointerArmed = pad.inputActive
            updateInput()
        }
        onUpdated: updateInput()
        onReleased: cancelInput()
        onCanceled: cancelInput()
    }
}
