/*
 *  This program create a lot of threads and execute them in an intensive
 * coordination way.
 *
 *  It's a valid program to test with valgrind that code is safe and correct on
 * threads
 *
 */

#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "lib/thread_seq.h"

// std::map<int, int> &ref_count_execs_thread() {
//  static auto result = new std::map<int, int>();
//  return *result;
//}

std::mutex m_keep_running;

void hard_work_sync(bool *keep_running, ts::ThreadSeq *ts, int /*id*/) {
  while (true) {
    {
      std::unique_lock<std::mutex> lk(m_keep_running);
      if (!*keep_running) {
        return;
      }
    }

    ts->run_sync([/*id*/] {
      //      std::cout << "running synchr........................." << id <<
      //      std::endl; ref_count_execs_thread()[id] += 1;
    });
    //    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  //  std::cout << "ended sync " << id << std::endl;
}

void hard_work_async(bool *keep_running, ts::ThreadSeq *ts, int /*id*/) {
  while (true) {
    {
      std::unique_lock<std::mutex> lk(m_keep_running);
      if (!*keep_running) {
        return;
      }
    }
    ts->run_async([/*id*/] {
      //      std::cout << "running Asynchr........................" << id <<
      //      std::endl; ref_count_execs_thread()[id] += 1;
    });
  }
  //  std::cout << "ended Async " << id << std::endl;
}

int main() {
  ts::ThreadSeq ts;

  std::vector<std::thread> threads;
  bool keep_running = true;
  for (int i = 0; i < 100; ++i) {
    threads.push_back(std::thread(hard_work_sync, &keep_running, &ts, i));
  }
  int n_th = threads.size();
  for (int i = 0; i < 100; ++i) {
    threads.push_back(
        std::thread(hard_work_async, &keep_running, &ts, i + n_th));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  //  ts.run_sync([] { std::cout << "finishing....." << std::endl; });
  {
    std::unique_lock<std::mutex> lk(m_keep_running);
    keep_running = false;
  }

  for (auto &th : threads) {
    th.join();
  }
  //  std::cout << "finished join....." << std::endl;
  //  for (const auto i : ref_count_execs_thread()) {
  //    std::cout << i.first << " " << i.second << std::endl;
  //  }
}

//#include <iostream>

//#include "lib/threadseq.h"

// int main() {
//  ts::ThreadSeq ts;

//  //---------------------------------

//  std::cout << "calling running synchr 1........................." <<
//  std::endl; ts.run_sync([] {
//    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
//    std::cout << "end running synchr 1........................." << std::endl;
//  });
//  std::cout << "after calling synchr 1........................." << std::endl;

//  //---------------------------------

//  std::cout << "calling running Asynchr ........................." <<
//  std::endl; ts.run_async([] {
//    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
//    std::cout << "end running Asynchr........................." << std::endl;
//  });
//  std::cout << "after calling Asynchr ........................." << std::endl;

//  //---------------------------------

//  std::cout << "calling running synchr 1........................." <<
//  std::endl; ts.run_sync([] {
//    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
//    std::cout << "end calling running synchr 1........................."
//              << std::endl;
//  });
//  std::cout << "after calling synchr 2........................." << std::endl;

//  //---------------------------------

//  std::cout << "stopping...." << std::endl;
//}
