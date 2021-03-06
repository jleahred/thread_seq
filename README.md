# ThreadSeq

Working with threads in 2021?

Pheeeew..., be careful.

The threads are dangerous, inefficient (memory and cpu), and do not scale.

But... in C++ we are a little behind with the concurrence subject, specially with the asynchrony.

So it's not rare to find libraries that use their own threads with callbacks from them (and sometimes from several, even the one created by oneself that has nothing to do with the library)

Other times, we need `asynchrony` and we don't have many options. It's sad to use threads for this, but...

And less often (especially if your system is microservice-oriented), we want a process to use more than one core.

So the threads are bad, but are there in `c++`, and will continue working with them for a while  :-(

(I miss erlang/elixir, even go, haskell. Even dart...)

Then, we have to deal with threads in c++.
How to coordinate them?

Well, there are no good standard general solutions.

The use of `mutex` or `semaphores` or `critical sections`... are not an option.

Of course, a good solution is messages passing and sharing little or nothing. But message passing is still a low-level mechanism. It requires a lot of awful code, type erasure for generic solutions, prior knowledge of types, bad performance...

And what if you want to run something on another thread and not continue until that execution is over?
What if you want a `request-response`  between threads? Passing messages? too much work, no please!


Here I propose a higher level and simple model.

The concept is to have a coordination or sequencing thread.

The rest of the threads will ask this thread to execute something on it.

No passing messages, no deletion of types, with casting.

You pass to this thread the code to be executed. As you can capture data in a lambda, you can pass code and context.
It sounds nice  :-)

Example

```cpp
#include <iostream>

#include "lib/threadseq.h"

int main() {

  ts::ThreadSeq ts;

  //---------------------------------

  std::cout << "calling running synchr 1................." << std::endl;
  ts.run_sync([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    std::cout << "end running synchr 1..................." << std::endl;
  });
  std::cout << "after calling synchr 1..................." << std::endl;

  //---------------------------------

  std::cout << "calling running Asynchr ................." << std::endl;
  ts.run_async([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    std::cout << "end running Asynchr...................." << std::endl;
  });
  std::cout << "after calling Asynchr ..................." << std::endl;

  //---------------------------------

  std::cout << "calling running synchr 1................." << std::endl;
  ts.run_sync([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    std::cout << "end calling running synchr 1..........." << std::endl;
  });
  std::cout << "after calling synchr 2..................." << std::endl;

  //---------------------------------

  std::cout << "stopping...." << std::endl;
}
```

First we create the thread for sequence the request `ThreadSeq`

```cpp

int main() {

  ts::ThreadSeq ts;

  ...
}
```

This call will create a thread and run it.

Later we request for a synchronous execution

```cpp

  //---------------------------------

  std::cout << "calling running synchr 1................." << std::endl;
  ts.run_sync([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    std::cout << "end running synchr 1..................." << std::endl;
  });
  std::cout << "after calling synchr 1..................." << std::endl;

  //---------------------------------

```

We are requesting it from the main thread, but it will be executed in `ThreadSeq`
And my code on main thread, will wait untill this code executed on `ThreadSeq` is finished

In next code, we will execute some code in `ThreadSeq`, but in an `asynchronous` way.

That's mean, the code will be executed on `ThreadSeq` as soon as possible, but my code on main thread, will continue the exeuction inmediatly

```cpp
  //---------------------------------

  std::cout << "calling running Asynchr ............." << std::endl;
  ts.run_async([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    std::cout << "end running Asynchr................" << std::endl;
  });
  std::cout << "after calling Asynchr ..............." << std::endl;

  //---------------------------------
```

The output the the full example is...

```txt
calling running synchr 1.........................
end running synchr 1.........................
after calling synchr 1.........................
calling running Asynchr .........................
after calling Asynchr .........................
calling running synchr 1.........................
end calling running synchr 1.........................
end running Asynchr.........................
after calling synchr 2.........................
stopping....
``` 

As you can see, callling async returns inmediatly and the code is executed later (as soon as possible)

Lines are not mixed, because all code to print lines is executed on same thread


The `main.cpp` is a program to coordinate in an intensive way in order to verify memory and concurrency safety with valgrind.

As you can see, the performance is bad and using all cores is bad

Well, it's threads, you know  :-(


## FAQ

#### Is it a header only  `library`?

Nop, but is a very small piece of code, it's very easy to do it if needed
