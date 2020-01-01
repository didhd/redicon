#include <iostream>
#include "redicon.h"

using namespace std;

int main(int argc, char* argv[]) {
  redicon::client client;

  client.connect("localhost", 6379);
  client.command("set", "hello", "world!", [] (const std::string &ret) {
      cout << ret << endl;
    });
  client.command("get", "hello", [&client] (const std::string &ret) {
      cout << "Hello, " << ret << endl;
      client.stop();
    });

  client.run();
  return 0;
}
