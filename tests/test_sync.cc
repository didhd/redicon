#include <iostream>
#include "redicon.h"
#include "gtest/gtest.h"

using namespace std;

TEST(test_sync, connect1) {
  redicon::client client;
  client.connect_sync("localhost", 6379);
  
  EXPECT_EQ(1, client.connections().size());
}


TEST(test_sync, connect2) {
  redicon::client client;
  client.connect_sync("localhost", 6379, 1);
  
  EXPECT_EQ(1, client.connections().size());
}

TEST(test_sync, connect3) {
  redicon::client client;
  client.connect_sync("localhost", 6379, 8);
  
  EXPECT_EQ(8, client.connections().size());
}


TEST(test_sync, set_variable) {
  redicon::client client;
  client.connect_sync("localhost", 6379);
  client.run_detached();

  std::string set_ret = client.command_sync("set", "test_variable", "test_value");
  EXPECT_EQ("+OK", set_ret);
}


TEST(test_sync, get_variable) {
  redicon::client client;
  client.connect_sync("localhost", 6379);
  client.run_detached();

  std::string get_ret = client.command_sync("get", "test_variable");  
  EXPECT_EQ("test_value", get_ret);
}


TEST(test_sync, set_and_get_variable) {
  redicon::client client;
  client.connect_sync("localhost", 6379);
  client.run_detached();

  std::string del_ret = client.command_sync("del", "hello");
  std::string set_ret = client.command_sync("set", "hello", "world!");
  std::string get_ret = client.command_sync("get", "hello");  
    
  EXPECT_EQ("+OK", set_ret);
  EXPECT_EQ("world!", get_ret); 
}


int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::off);

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
