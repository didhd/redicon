#include "redicon.h"

#include <iostream>
#include <chrono>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

int main(int argc, char **argv) {
  try {
    redicon::client client;

    std::cout << "Running test in: 2 threads, 1 redis connections.\n";
    client.connect_sync("localhost", 6379);
    client.run_detached();

    auto start = std::chrono::steady_clock::now();
    std::string ret;

    int i;
    for (i = 1; i <= 100000; i++) {
      if (i == 100000) {
        ret = client.command_sync("get", std::to_string(i));
        break;
      }
      else {
        client.command_sync("get", std::to_string(i));
      }
    }

    std::cout << i << " => reply: " << ret << std::endl;

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = end - start;

    std::cout << i << " commands in " << elapsed.count() << " seconds\n";
    std::cout << "get : " << (i / elapsed.count()) << " commands/s\n";
  }
  catch (std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
  }

  return 0;
}
