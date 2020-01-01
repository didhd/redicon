#include <iostream>
#include "redicon.h"
#include "gtest/gtest.h"

using namespace std;

TEST(test_async, connect1) {
  redicon::client client;
  client.connect("localhost", 6379);
  
  EXPECT_EQ(1, client.connections().size());
}


TEST(test_async, connect2) {
  redicon::client client;
  client.connect("localhost", 6379, 1);
  
  EXPECT_EQ(1, client.connections().size());
}

TEST(test_async, connect3) {
  redicon::client client;
  client.connect("localhost", 6379, 8);
  
  EXPECT_EQ(8, client.connections().size());
}


TEST(test_async, set_variable) {
  redicon::client client;
  client.connect("localhost", 6379);

  std::string set_ret;
  client.command("set", "test_variable", "test_value", [&client, &set_ret] (const std::string &ret) {
    set_ret = ret;
    client.stop();
  });

  client.run();
  
  EXPECT_EQ("+OK", set_ret);
}


TEST(test_async, get_variable) {
  redicon::client client;
  client.connect("localhost", 6379);

  std::string get_ret;
  client.command("get", "test_variable", [&client, &get_ret] (const std::string &ret) {
    get_ret = ret;
    client.stop();
  });

  client.run();
  
  EXPECT_EQ("test_value", get_ret);
}


TEST(test_async, set_and_get_variable) {
  redicon::client client;
  client.connect("localhost", 6379);

  std::string set_ret, get_ret;
  client.command("set", "hello", "world!", [&set_ret] (const std::string &ret) {
    set_ret = ret;
  });
  client.command("get", "hello", [&client, &get_ret] (const std::string &ret) {
    get_ret = ret;
    client.stop();
  });
  client.run();
  
  EXPECT_EQ("+OK", set_ret);
  EXPECT_EQ("world!", get_ret); 
}


int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::off);

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
