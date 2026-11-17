#pragma once
#include <condition_variable>
#include <atomic>
#include <deque>
#include <future>
#include <functional>
#include <linux/can.h>
#include <mutex>
#include <thread>
#include <sys/types.h>
namespace rnet::qt {
// One writer thread waits for queued frames. The same mutex serializes its
// writes with reads and socket replacement in the GUI thread.
class CanSendQueue final {
public:
  struct Result { ssize_t bytes=-1; int error=0; };
  CanSendQueue();
  ~CanSendQueue();
  CanSendQueue(const CanSendQueue &)=delete;
  CanSendQueue &operator=(const CanSendQueue &)=delete;
  void attach(int fd);
  void detach();
  bool inspect(const std::function<bool(int)> &check);
  ssize_t read(can_frame &frame);
  Result send(const can_frame &frame);
private:
  struct Request {
    can_frame frame{};
    unsigned long long generation=0;
    std::promise<Result> completion;
  };
  std::mutex queueMutex_,ioMutex_;
  std::condition_variable ready_;
  std::deque<Request> queue_;
  int fd_=-1;
  std::atomic<unsigned long long> generation_{0};
  bool stopping_=false;
  std::thread writer_;
  void run();
};
}
