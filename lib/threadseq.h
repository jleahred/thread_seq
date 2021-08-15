#pragma once

#include <functional>
#include <memory>

namespace ts {

class ThreadSeq;
struct Impl;
} // namespace ts

namespace ts {

class cfg {
public:
    cfg();

    //  over this limit, the run_async will be sttoped till reduce the queue
    cfg set_max_sync_queue_size(unsigned s);

    //  lambda wrapper
    //  it could be a good idea to use it in order to control all possible exceptions
    cfg set_lambda_wrapper(std::function<void(std::function<void(void)>)> wr);

private:
    unsigned                                       max_sync_queue_size;
    std::function<void(std::function<void(void)>)> lambda_wrapper;

    friend class ThreadSeq;
    friend void loop(Impl* ts);
};

class ThreadSeq {
public:
    ThreadSeq(cfg c = cfg());
    ~ThreadSeq();

    void run_sync(std::function<void(void)> fn); //  It can throws ExceptionDeadLock
    void run_async(std::function<void(void)> fn);

    void stop();

private:
    std::shared_ptr<Impl> impl;

    //    friend void loop(ThreadSeq* ts);
}; // namespace ts

//  Errors

//  ExceptionDeadLock
//  It will be thrown if a dead lock is detected on run_sync
class ExceptionDeadLock {
};

} // namespace ts
