import QtQuick
import QtTest
import "../../qt_attendant/qml" as UI

TestCase {
    name: "CapturePlaybackControls"
    when: windowShown
    property var app
    Component { id: factory; UI.CapturePlayback {} }
    function initTestCase() { app = factory.createObject(null); verify(app !== null); wait(100) }
    function cleanupTestCase() { app.close(); app.destroy() }
    function item(name) { return findChild(app.contentItem, name) }
    function test_event_navigation_and_seek() {
        compare(app.position, 0)
        mouseClick(item("nextEvent"))
        compare(app.position, 5)
        compare(app.current.owner, 0)
        mouseClick(item("nextEvent"))
        compare(app.position, 20)
        compare(app.current.phase, "Off")
        mouseClick(item("previousEvent"))
        compare(app.position, 5)
        app.seek(12)
        compare(app.current.x, 50)
        compare(app.current.y, -20)
        app.seek(-10)
        compare(app.position, 0)
    }
    function test_play_pause_and_restart() {
        app.seek(20)
        mouseClick(item("playPause"))
        tryVerify(() => app.position > 0 && app.position < 5)
        mouseClick(item("playPause"))
        const paused = app.position
        wait(100)
        compare(app.position, paused)
    }
}
