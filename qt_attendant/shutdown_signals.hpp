#pragma once
#include <QObject>
#include <functional>
#include <signal.h>

class QSocketNotifier;
namespace attendant {
// Signal handler writes only to a nonblocking pipe; window shutdown runs on Qt.
class ShutdownSignals final : public QObject {
public:
    explicit ShutdownSignals(QObject *parent=nullptr):QObject(parent){}
    ~ShutdownSignals() override;
    bool install(std::function<void()> closeWindow);
private:
    int read_=-1, write_=-1;
    bool installed_=false;
    struct sigaction previousInt_{}, previousTerm_{};
    QSocketNotifier *notifier_=nullptr;
};
}
