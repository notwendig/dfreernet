#include "can_send_queue.hpp"
#include <cerrno>
#include <unistd.h>
namespace rnet::qt {
CanSendQueue::CanSendQueue():writer_([this]{run();}){}
CanSendQueue::~CanSendQueue(){
  {std::lock_guard<std::mutex> lock(queueMutex_);stopping_=true;}
  ready_.notify_one();
  writer_.join();
}
void CanSendQueue::attach(int fd){
  std::lock_guard<std::mutex> lock(ioMutex_);
  fd_=fd;++generation_;
}
void CanSendQueue::detach(){
  std::lock_guard<std::mutex> lock(ioMutex_);
  fd_=-1;++generation_;
}
bool CanSendQueue::inspect(const std::function<bool(int)> &check){
  std::lock_guard<std::mutex> lock(ioMutex_);
  return fd_>=0&&check(fd_);
}
ssize_t CanSendQueue::read(can_frame &frame){
  std::lock_guard<std::mutex> lock(ioMutex_);
  if(fd_<0){errno=EBADF;return -1;}
  return ::read(fd_,&frame,sizeof(frame));
}
CanSendQueue::Result CanSendQueue::send(const can_frame &frame){
  Request request;request.frame=frame;request.generation=generation_.load();
  auto completed=request.completion.get_future();
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if(stopping_)return {-1,ECANCELED};
    queue_.push_back(std::move(request));
  }
  ready_.notify_one();
  return completed.get();
}
void CanSendQueue::run(){
  for(;;){
    Request request;
    bool stop=false;
    {
      std::unique_lock<std::mutex> lock(queueMutex_);
      ready_.wait(lock,[this]{return stopping_||!queue_.empty();});
      if(queue_.empty())break;
      request=std::move(queue_.front());queue_.pop_front();
      stop=stopping_;
    }
    Result result{-1,ECANCELED};
    if(!stop){
      std::lock_guard<std::mutex> lock(ioMutex_);
      if(fd_<0||request.generation!=generation_.load())result.error=ENODEV;
      else{
        result.bytes=::write(fd_,&request.frame,sizeof(request.frame));
        result.error=result.bytes==ssize_t(sizeof(request.frame))?0:errno;
      }
    }
    request.completion.set_value(result);
  }
}
}
