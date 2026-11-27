#include "shutdown_signals.hpp"
#include <QSocketNotifier>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace attendant {
namespace {
volatile sig_atomic_t closePipe=-1;
void requestClose(int signal)
{
    const int saved=errno;
    const auto fd=closePipe;
    if(fd>=0) {
        const unsigned char byte=static_cast<unsigned char>(signal);
        const auto ignored=::write(fd,&byte,1);
        (void)ignored;
    }
    errno=saved;
}
}
ShutdownSignals::~ShutdownSignals()
{
    if(notifier_)notifier_->setEnabled(false);
    if(installed_) {
        closePipe=-1;
        ::sigaction(SIGINT,&previousInt_,nullptr);
        ::sigaction(SIGTERM,&previousTerm_,nullptr);
    }
    if(read_>=0)::close(read_);
    if(write_>=0)::close(write_);
}
bool ShutdownSignals::install(std::function<void()> closeWindow)
{
    if(installed_ || closePipe>=0)return false;
    int pipe[2];
    if(::pipe2(pipe,O_NONBLOCK|O_CLOEXEC)!=0)return false;
    read_=pipe[0];write_=pipe[1];
    struct sigaction action{};
    action.sa_handler=requestClose;
    ::sigemptyset(&action.sa_mask);
    action.sa_flags=SA_RESTART;
    if(::sigaction(SIGINT,&action,&previousInt_)!=0)return false;
    if(::sigaction(SIGTERM,&action,&previousTerm_)!=0) {
        ::sigaction(SIGINT,&previousInt_,nullptr);
        return false;
    }
    closePipe=write_;
    installed_=true;
    notifier_=new QSocketNotifier(read_,QSocketNotifier::Read,this);
    QObject::connect(notifier_,&QSocketNotifier::activated,this,[this,closeWindow=std::move(closeWindow)]{
        unsigned char bytes[64];
        while(::read(read_,bytes,sizeof(bytes))>0){}
        closeWindow();
    });
    return true;
}
}
