#ifndef THREADSEQ_H
#define THREADSEQ_H

#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

namespace ts {

class ThreadSeq {
public:
  ThreadSeq();
  ~ThreadSeq();

  void run_sync(std::function<void(void)> fn);
  void run_async(std::function<void(void)> fn);

private:
  bool keep_running = true;
  std::shared_ptr<std::thread> th;
  std::mutex m_global;
  std::condition_variable cv;
  std::mutex m;

  std::shared_ptr<std::function<void(void)>> lambda_sync;
  std::list<std::function<void(void)>> lambda_async;

  friend void loop(ThreadSeq *ts);
}; // namespace ts

} // namespace ts

#endif // THREADSEQ_H
