#include "redicon.h"

#include <iostream>
#include <vector>
#include <boost/lexical_cast.hpp>

int main(int argc, char **argv) {
  try {
    redicon::client client;
    client.connect("localhost", 6379, 8);

    // Run the io_context on the requested number of consumer threads.
    const int worker_count = 4;
    std::vector<std::thread> workers;
    workers.reserve(worker_count - 1);
    for(auto i = worker_count - 1; i > 0; --i)
      workers.emplace_back([&client]{ client.run(); });

    std::cout << "Running test in: 4 thread, 8 redis connection." << std::endl;

    // Main producer thread.
    const int command_count = 1000000;
    auto start = std::chrono::steady_clock::now();

    for (int i = 1; i <= command_count; i++) {
      client.command("get", boost::lexical_cast<std::string>(i), [i, &start](const std::string &ret) {
        if (i == command_count) {
          auto end = std::chrono::steady_clock::now();
          std::chrono::duration<float> elapsed = end - start;

          std::cout << boost::lexical_cast<std::string>(i) + " => reply: " << ret << std::endl;
          std::cout << boost::lexical_cast<std::string>(command_count) << " commands in " << elapsed.count() << " seconds" << std::endl;
          std::cout << "get : " << (command_count / elapsed.count()) << " commands/s" << std::endl;
        }
        if (boost::lexical_cast<std::string>(i) != ret) {
          std::cout << boost::lexical_cast<std::string>(i) + " => reply: " << ret << std::endl;
        }
      });
    }
    client.run();
  }
  catch (std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
  }

  return 0;
}
