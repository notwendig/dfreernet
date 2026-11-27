#include "attendant_controller.hpp"
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>
class AttendantSetup : public QObject {
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine){
        engine->rootContext()->setContextProperty("captureDuration", 20.0);
        engine->rootContext()->setContextProperty("captureEvents", QVariantList{
            QVariantMap{{"time", 0.0}, {"phase", "Start"}, {"owner", -1}, {"x", 0}, {"y", 0}},
            QVariantMap{{"time", 5.0}, {"phase", "Confirmed"}, {"owner", 0}, {"x", 0}, {"y", 0}},
            QVariantMap{{"time", 10.0}, {"phase", "Joystick"}, {"owner", 0}, {"x", 50}, {"y", -20}},
            QVariantMap{{"time", 20.0}, {"phase", "Off"}, {"owner", -1}, {"x", 0}, {"y", 0}}
        });
        auto *controller=new attendant::AttendantController(engine);
        controller->registerOnDataFrame();
        engine->rootContext()->setContextProperty("controller",controller);
    }
};
QUICK_TEST_MAIN_WITH_SETUP(attendant_ui,AttendantSetup)
#include "attendant_quick_test.moc"
