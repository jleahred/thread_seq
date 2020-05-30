#include "threadseq.h"

namespace {} // namespace

namespace ts {

void loop(ts::ThreadSeq *ts);

ThreadSeq::ThreadSeq() { th = std::make_shared<std::thread>(loop, this); }
ThreadSeq::~ThreadSeq() {
  this->keep_running = false;
  th->join();
}

void ThreadSeq::run_sync(std::function<void(void)> fn) {
  std::lock_guard<std::mutex> global(this->m_global);
  //    full method runs from external thread
  //

  //  put lambda and notify
  {
    std::lock_guard<std::mutex> lk(this->m);
    this->lambda_sync = std::make_shared<std::function<void(void)>>(fn);
    this->cv.notify_one();
  }

  // wait for execution on threadseq thread (the ThreadSeq finished)
  {
    std::unique_lock<std::mutex> lk(this->m);
    this->cv.wait(lk, [this] { return lambda_sync.get() == 0; });
  }
}

void ThreadSeq::run_async(std::function<void(void)> fn) {
  std::lock_guard<std::mutex> global(this->m_global);
  //    full method runs from external thread
  //

  //  put lambda_async and notify
  {
    std::lock_guard<std::mutex> lk(this->m);
    this->lambda_async.push_back(fn);
    this->cv.notify_one();
  }
}

void loop(ts::ThreadSeq *ts) {
  while (ts->keep_running) {
    std::unique_lock<std::mutex> lk(ts->m);
    ts->cv.wait_for(lk, 100ms, [ts] { return ts->lambda_sync.get() != 0; });
    //    std::cout << "Worker thread working\n";
    if (ts->lambda_sync) {
      (*ts->lambda_sync)();

      ts->cv.notify_all();
      ts->lambda_sync.reset();
    }
    {
      while (!ts->lambda_async.empty()) {
        auto la = ts->lambda_async.front();
        ts->lambda_async.pop_front();
        la();
      }
    }
    lk.unlock();
  }
}

} // namespace ts
