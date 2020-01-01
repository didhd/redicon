#include "redicon.h"

#include <iostream>
#include <chrono>
#include <boost/lexical_cast.hpp>

int main(int argc, char **argv) {
  try {
    redicon::client client;

    std::cout << "Running test in: 1 thread, 1 redis connection.\n";
    client.connect("localhost", 6379, 1);


    auto start = std::chrono::steady_clock::now();

    for (int i = 1; i <= 1000000; i++)
      client.command("get", std::to_string(i), [i, &start](const std::string &ret) {
        if (i == 1000000) {
          auto i_str = boost::lexical_cast<std::string>(i);
          std::cout << i_str + " => reply: " << ret << std::endl;

          auto end = std::chrono::steady_clock::now();
          std::chrono::duration<float> elapsed = end - start;

          std::cout << "1000000 commands in " << elapsed.count() << " seconds\n";
          std::cout << "get : " << (1000000 / elapsed.count()) << " commands/s\n";
        }
      });

    client.run();
  }
  catch (std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
  }

  return 0;
}
