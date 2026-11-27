#pragma once
#include <QString>
#include <QVariantList>

namespace attendant {
struct CapturePlayback {
    QVariantList events;
    double duration = 0;
    int frames = 0;
    QString error;
};
CapturePlayback readCapture(const QString &path);
}
