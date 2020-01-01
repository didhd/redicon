# redicon
Boost.Asio based asynchronous, thread-safe and very fast C++11 [Redis](http://redis.io/) client.

**Features:**

 * Asynchronous API Call
 * Multi Threaded & Thread-safe
 * Very Fast

## Benchmarks
Benchmarks were performed on Ubuntu 16.04 (64-bit) and a local Redis server.
 * `benchmark_async_multi` over local redis server: **924,653 commands/s**
 * `benchmark_async` over local redis server: **672,462 commands/s**

## Examples
Main usage of redicon. See `examples/` for more usages.

#### Hello, world!
Asynchronous redicon client:

```c++
#include <iostream>
#include "redicon.h"

using namespace std;

int main(int argc, char* argv[]) {
  boost::asio::io_context io_context;
  redicon::client client(io_context);

  client.connect("localhost", 6379);
  client.command("set", "hello", ", world!", [] (const std::string &ret) {
      cout << ret << endl;
    });
  client.command("get", "hello", [] (const std::string &ret) {
      cout << ret << endl;
    });

  io_context.run();
  return 0;
}
```

Compile and run:

    $ g++ hello.cc -o hello -std=c++11 -lredicon
    $ ./hello
    +OK
    Hello, world!

This example is asynchronous, which means the commands will return immediately
after each operation is started. Callback function will be called later when
the server sends reply.

#### Hello, world!
Synchronous redicon client:

```c++
#include <iostream>
#include "redicon.h"

using namespace std;

int main(int argc, char* argv[]) {
  redicon::client client;

  client.connect_sync("localhost", 6379);
  client.run_detached();

  client.command_sync("set", "hello", "world");
  std::string ret_sync = client.command_sync("get", "hello");
  cout << "Hello, " << ret_sync << endl;

  return 0;
}
```

Compile and run:

    $ g++ hello.cc -o hello -std=c++11 -lredicon
    $ ./hello
    Hello, world!

This example is synchronous, which means the commands will have to wait
until operation is complete.

## Installation
Instructions provided are for CentOS, but all components are platform-independent.

#### Build from source
Get the build environment and dependencies:

    sudo yum -y install git cmake3 build-essential
    sudo yum -y install openssl-static
    wget https://dl.bintray.com/boostorg/release/1.66.0/source/boost_1_66_0.tar.gz
    tar -xvzf boost_1_66_0.tar.gz
    cd boost_1_66_0
    ./bootstrap.sh
    sudo ./b2 install

Build the library:

    mkdir build && cd build
    cmake3 ..
    make

#### Build examples
Enable examples using cmake or the following:

    cmake3 -DBUILD_EXAMPLES=ON ..
    make examples

#### Build documentation
You can make documentation with [doxygen](http://doxygen.org/):

    cd docs
    doxygen

## License
The MIT License (MIT)

Copyright (c) 2019 Sanghwa Na.
