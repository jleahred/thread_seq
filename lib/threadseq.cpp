#include "thread_seq.h"

#include <chrono>
#include <iostream>
#include <set>

#include <atomic>
#include <condition_variable>
#include <list>
#include <map>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

namespace {
const int DEFAULT_MAX_QUEUE_SIZE = 100;
} // namespace

namespace ts {

void loop(Impl* ts);

cfg::cfg()
    : max_sync_queue_size(DEFAULT_MAX_QUEUE_SIZE)
    , lambda_wrapper([](std::function<void(void)> l) {
        try {
            l();
        } catch (...) {
            abort(); //  todo: do something diferent
        }
    })
{
}

cfg cfg::set_max_sync_queue_size(unsigned s)
{
    this->max_sync_queue_size = s;
    return *this;
}

cfg cfg::set_lambda_wrapper(std::function<void(std::function<void(void)>)> wr)
{
    this->lambda_wrapper = wr;
    return *this;
}

class DeadLockDetector {
public:
    DeadLockDetector();

    void register_sync_call(std::thread::id caller_thread_id);
    void unregister_sync_call(std::thread::id caller_thread_id);

private:
    std::mutex                     m;
    std::map<std::thread::id, int> map_sync_waitting; //  count by thread_id sync queued
    std::vector<std::thread::id>   stack_sync_waitting;

    std::thread::id last_locked_threadid();
};

struct Impl {
    const cfg               config;
    std::atomic<bool>       keep_running; //   = true; c++20
    std::mutex              m_lambdas;
    std::condition_variable cv_process_pendings;
    std::mutex              m_cv_process_pendings;
    std::mutex              m_running_sync;
    std::condition_variable cv_sync_finished;
    std::mutex              m_cv_sync_finished;

    DeadLockDetector dld;

    std::shared_ptr<std::function<void(void)>> lambda_sync;
    std::list<std::function<void(void)>>       lambda_async;

    std::shared_ptr<std::thread> th;
    //    std::thread th;

    Impl(cfg c)
        : config(c)
        , keep_running(true)
    //, th(loop, this)
    {
        //  we have to init this way to keep valgrind happy  :-O
        this->cv_process_pendings.notify_one();
        this->cv_sync_finished.notify_one();
        th = std::make_shared<std::thread>(loop, this);
    }

    ~Impl()
    {
        this->stop();
        th->join();
    }

    void stop()
    {
        if (this->keep_running) {
            this->keep_running = false;
            std::cout << "stopping thread" << std::endl;
        }
    }
};

ThreadSeq::ThreadSeq(cfg c)
    : impl(new Impl(c))
{
}
ThreadSeq::~ThreadSeq()
{
}

void ThreadSeq::stop()
{
    impl->stop();
}

void ThreadSeq::run_sync(std::function<void(void)> fn)
{
    if (!impl->keep_running) {
        return;
    }

    const auto caller_thread_id = std::this_thread::get_id();
    if (caller_thread_id == impl->th->get_id()) {
        fn();
        return;
    }

    {
        //  dead lock detection
        this->impl->dld.register_sync_call(caller_thread_id);
    }

    //  wait till async available
    impl->m_running_sync.lock();
    if (!impl->keep_running) {
        impl->m_running_sync.unlock();
        this->impl->dld.unregister_sync_call(caller_thread_id);
        return;
    }

    {
        //  put lambda_async and notify
        impl->m_lambdas.lock();
        impl->lambda_sync = impl->lambda_sync = std::make_shared<std::function<void(void)>>(fn);
        impl->m_lambdas.unlock();
        impl->cv_process_pendings.notify_one();
    }

    {
        //  wait for finished lambda call
        std::unique_lock<std::mutex> lk(impl->m_cv_sync_finished);
        while (!impl->cv_sync_finished.wait_for(lk, 10ms, [this] {
            std::lock_guard<std::mutex> lock(this->impl->m_lambdas);
            const auto                  stop_waitting = !this->impl->lambda_sync || !this->impl->keep_running;
            return stop_waitting;
        })) {
            if (!impl->keep_running) {
                impl->m_running_sync.unlock();
                this->impl->dld.unregister_sync_call(caller_thread_id);
                return;
            }
        }
    }

    if (impl->keep_running && impl->lambda_sync) {
        abort(); //  todo: do something diferent
    }
    this->impl->dld.unregister_sync_call(caller_thread_id);
    impl->m_running_sync.unlock();
}

void ThreadSeq::run_async(std::function<void(void)> fn)
{
    if (!impl->keep_running) {
        return;
    }

    {
        //  wait for available space
        while (impl->keep_running) {
            impl->m_lambdas.lock();
            if (impl->lambda_async.size() < impl->config.max_sync_queue_size && impl->keep_running) {
                impl->m_lambdas.unlock();
                break;
            }
            impl->m_lambdas.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    {
        //  put lambda_async and notify
        impl->m_lambdas.lock();
        impl->lambda_async.push_back(fn);
        impl->m_lambdas.unlock();
        impl->cv_process_pendings.notify_one();
    }
}

void loop(ts::Impl* ts)
{
    while (ts->keep_running) {
        {
            //  wait for notification lambdas ready
            std::unique_lock<std::mutex> lk_process_pendings(ts->m_cv_process_pendings);
            while (!ts->cv_process_pendings.wait_for(lk_process_pendings, 100ms, [&ts] {
                //                auto       lock_lambdas  = std::lock_guard(ts->m_lambdas);    //  c++20
                std::lock_guard<std::mutex> lock_lambdas(ts->m_lambdas);
                const auto                  stop_waitting = ts->lambda_sync.get() || ts->lambda_async.size() || !ts->keep_running;
                return stop_waitting;
            })) {
                if (!ts->keep_running)
                    continue;
            }
        }

        {
            // Sync
            ts->m_lambdas.lock();
            if (ts->keep_running && ts->lambda_sync) {
                auto la = *ts->lambda_sync;
                ts->m_lambdas.unlock();
                {
                    try {
                        ts->config.lambda_wrapper(la);
                        //                    la();
                    } catch (...) {
                        abort(); //  todo: do something diferent
                    }
                }
                ts->m_lambdas.lock();
                ts->lambda_sync.reset();
            }
            ts->m_lambdas.unlock();
            ts->cv_sync_finished.notify_one();
        }

        {
            // Async
            ts->m_lambdas.lock();
            while (ts->keep_running && !ts->lambda_async.empty()) {
                auto la = ts->lambda_async.front();
                ts->lambda_async.pop_front();
                ts->m_lambdas.unlock();
                {
                    try {
                        ts->config.lambda_wrapper(la);
                        //                    la();
                    } catch (...) {
                        abort(); //  todo: do something diferent
                    }
                }
                ts->m_lambdas.lock();
            }

            ts->m_lambdas.unlock();
        }
    }
}

DeadLockDetector::DeadLockDetector()
{
}

std::thread::id
DeadLockDetector::last_locked_threadid()
{
    //  m.lock();  not needed, it will be called always with this lock
    if (stack_sync_waitting.empty())
        return std::thread::id(0);
    auto result = stack_sync_waitting.back();
    stack_sync_waitting.pop_back();
    return result;
}

void DeadLockDetector::register_sync_call(std::thread::id caller_thread_id)
{
    m.lock();
    if (map_sync_waitting[caller_thread_id] != 0 && caller_thread_id != last_locked_threadid()) {
        m.unlock();
        throw ExceptionDeadLock{};
    }
    map_sync_waitting[caller_thread_id] = map_sync_waitting[caller_thread_id] + 1;
    stack_sync_waitting.push_back(caller_thread_id);
    m.unlock();
}

void DeadLockDetector::unregister_sync_call(std::thread::id caller_thread_id)
{
    m.lock();
    if (map_sync_waitting[caller_thread_id] > 0) {
        map_sync_waitting[caller_thread_id] = map_sync_waitting[caller_thread_id] - 1;
        if (map_sync_waitting[caller_thread_id] == 0) {
            map_sync_waitting.erase(caller_thread_id);
        }
        stack_sync_waitting.pop_back();
        m.unlock();
    } else {
        abort(); //  todo: do something diferent
    }
}

} // namespace ts
