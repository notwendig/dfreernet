import QtQuick
import QtQuick.Controls
import QtTest
import "../../qt_attendant/qml" as UI

TestCase {
    name: "AttendantControls"
    when: windowShown
    property var app
    Component { id: factory; UI.Main {} }
    function item(name) { return findChild(app.contentItem, name) }
    function initTestCase() { app = factory.createObject(null); wait(80) }
    function cleanupTestCase() { app.close(); app.destroy() }
    function init() { controller.stop(); wait(30) }
    function cleanup() { controller.stop() }
    function test_takeover_and_return() {
        mouseClick(item("takeControlButton"))
        compare(controller.attendantControl, true)
        compare(controller.held, true)
        controller.setAxes(60, 0)
        verify(controller.outputX > 0)
        mouseClick(item("takeControlButton"))
        compare(controller.attendantControl, false)
        compare(controller.outputX, 0)
        compare(controller.outputY, 0)
    }
    function test_mouse_release_and_speed_rearm() {
        mouseClick(item("takeControlButton"))
        const pad = item("joystickPad")
        mousePress(pad, pad.width/2, pad.height/2)
        mouseMove(pad, pad.width/2, pad.height/4, 20)
        tryVerify(() => controller.outputY > 0)
        mouseRelease(pad, pad.width/2, pad.height/4)
        compare(controller.outputY, 0)
        controller.setAxes(0, 80)
        mouseClick(item("speedButton"))
        compare(controller.outputY, 0)
        controller.setAxes(0, 0)
        controller.setAxes(0, 50)
        verify(controller.outputY > 0)
    }
    function test_held_pointer_and_neutral_remain_fresh() {
        mouseClick(item("takeControlButton"))
        wait(350)
        verify(controller.attendantControl)
        const pad = item("joystickPad")
        mousePress(pad, pad.width/2, pad.height/2)
        mouseMove(pad, pad.width/2, pad.height/4, 20)
        tryVerify(() => controller.outputY > 0)
        wait(350)
        verify(controller.attendantControl)
        verify(controller.outputY > 0)
        mouseRelease(pad, pad.width/2, pad.height/4)
        compare(controller.outputY, 0)
    }
    function test_keyboard_button_activation() {
        const mode = controller.modeNumber
        item("profileButton").forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(controller.profile, 8)
        compare(controller.modeNumber, mode % 2 + 1)
    }
    function test_focus_keeps_control_without_resuming_held_pointer() {
        mouseClick(item("takeControlButton"))
        const pad = item("joystickPad")
        const mode = controller.modeNumber
        const speed = controller.inputLimit
        mousePress(pad, pad.width/2, pad.height/2)
        mouseMove(pad, pad.width/2, pad.height/4, 20)
        tryVerify(() => controller.outputY > 0)
        try {
            pad.inputActive = false
            compare(controller.outputY, 0)
            wait(350)
            verify(controller.attendantControl)
            compare(controller.modeNumber, mode)
            compare(controller.inputLimit, speed)
            pad.inputActive = true
            wait(100)
            compare(controller.outputY, 0)
            verify(controller.attendantControl)
            mouseRelease(pad, pad.width/2, pad.height/4)
            mousePress(pad, pad.width/2, pad.height/2)
            mouseMove(pad, pad.width/2, pad.height/4, 20)
            tryVerify(() => controller.outputY > 0)
            mouseRelease(pad, pad.width/2, pad.height/4)
        } finally {
            pad.inputActive = Qt.binding(() => pad.Window.active)
        }
    }
    function test_profile_speed_cycles() {
        const mode = controller.modeNumber
        mouseClick(item("profileButton"))
        compare(controller.profile, 8)
        compare(controller.modeNumber, mode % 2 + 1)
        const speed = controller.inputLimit
        mouseClick(item("speedButton"))
        compare(controller.inputLimit, speed >= 100 ? 0 : (Math.floor(speed / 20) + 1) * 20)
        let minimumSeen = controller.inputLimit === 0
        for (let i = 0; i < 6; ++i) {
            mouseClick(item("speedButton"))
            minimumSeen = minimumSeen || controller.inputLimit === 0
        }
        verify(minimumSeen)
    }
}
