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
