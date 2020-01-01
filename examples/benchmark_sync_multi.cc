#include "redicon.h"

#include <iostream>
#include <chrono>
#include <boost/lexical_cast.hpp>

int main(int argc, char **argv) {
  try {
    redicon::client client;

    std::cout << "Running test in: 4 thread, 8 redis connection.\n";
    client.connect_sync("localhost", 6379, 8);

    auto start = std::chrono::steady_clock::now();
    std::string ret;

    // consumer threads
    size_t consumer_count = 3;
    for(size_t t = 0; t < consumer_count; ++t) {
      client.run_detached();
    }

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

    client.run();
  }
  catch (std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
  }

  return 0;
}
