#ifndef REDICON_REDICON_H
#define REDICON_REDICON_H

#include <queue>
#include <mutex>
#include <random>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include "spdlog/spdlog.h"

namespace redicon {

using boost::asio::ip::tcp;
using boost::system::error_code;
using session = std::shared_ptr<tcp::socket>;

class connection : public std::enable_shared_from_this<connection> {
private:
  session session_;
  bool stopped_ = false;
  std::vector<char> rbuf_;
  std::string stored_string_;

  std::mutex mtx_;
  boost::asio::io_context::strand strand_;
  boost::asio::steady_timer deadline_;
  boost::asio::steady_timer heartbeat_timer_;
  boost::asio::steady_timer reconnect_timer_;
  
  std::queue<std::function<void(const std::string &)>> handlers_;
  std::string local_endpoint_;
  tcp::resolver::results_type endpoints_;

public:
  // constructor
  explicit connection(boost::asio::io_context &io_context);

  void check_deadline();

  void close();

  const std::string local_endpoint() const {
    return local_endpoint_;
  }

  tcp::resolver::results_type& endpoints() {
    return endpoints_;
  }

  // data member accessing function
  boost::asio::io_context::strand strand() {
    return strand_;
  }

  inline bool is_open() const {
    if (session_->is_open())
      return true;
    return false;
  }

  void do_connect(tcp::resolver::results_type::iterator endpoint_iterator);

  void handle_connect(const error_code &ec, tcp::resolver::results_type::iterator endpoint_iterator);

  error_code do_sync_connect(tcp::resolver::results_type::iterator endpoint_iterator);

  error_code do_temp_connect(tcp::resolver::results_type::iterator endpoint_iterator);

  void do_reconnect(tcp::resolver::results_type::iterator endpoint_iterator);

  void start_ping_sync();

  // stranded function for do_session.
  void start_session(const std::string &message, std::function<void(const std::string &)> handler);

  // send a string message.
  void do_session(const std::string &message, std::function<void(const std::string &)> handler);

  std::string do_session_sync(const std::string &message);

  void handle_write(const error_code &ec, size_t bytes, std::string message);

  // keep send ping messages asynchronously.
  void start_ping();

  void send_ping();

  void on_ping(const error_code &ec, size_t);

  void handle_heartbeat(const std::string &message);

  // keep receive messages and invoke first handler in queue.
  void start_read();

  void handle_read(const error_code &ec, size_t bytes_read);
};

//------------------------------------------------------------------------------

// random selector for connection class.
template <typename RandomGenerator = std::default_random_engine>
struct random_selector {
	random_selector(RandomGenerator g = RandomGenerator(std::random_device("/dev/urandom")()))
		: gen(g) {}

	template <typename Iter>
	Iter select(Iter start, Iter end) {
		std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
		std::advance(start, dis(gen));
		return start;
	}

	//convenience function
	template <typename Iter>
	Iter operator()(Iter start, Iter end) {
		return select(start, end);
	}

	//convenience function that works on anything with a sensible begin() and end(), and returns with a ref to the value type
	template <typename Container>
	auto operator()(const Container& c) -> decltype(*begin(c))& {
		return *select(begin(c), end(c));
	}

private:
	RandomGenerator gen;
};

//------------------------------------------------------------------------------

class client : public std::enable_shared_from_this<client> {
private:
  boost::asio::io_context io_context_;
  boost::asio::io_context::strand strand_;

  std::vector<std::shared_ptr<connection>> connections_;
  random_selector<> selector_;

public:
  // constructor
  explicit client();

  client(boost::asio::io_context &io_context);
  
  // create a connection and connects to redis.
  void connect(const std::string &ip, const int &port);

  // create multiple connections and connects to redis.
  void connect(const std::string &hostname, const int &port, size_t count);

  void connect_sync(const std::string &ip, const int &port);

  void connect_sync(const std::string &hostname, const int &port, size_t count);

  void run();

  void run_detached();

  void run_detached(int concurrency_hint);

  void stop();

  // send commands.
  void command(const std::string &cmd, std::function<void(const std::string &)> handler);

  void command(const std::string &cmd, const std::string &arg, std::function<void(const std::string &)> handler);

  void command(const std::string &cmd, const std::string &arg1, const std::string &arg2, std::function<void(const std::string &)> handler);

  // send command with string of initializer_list.
  void command(const std::initializer_list<std::string> &args, std::function<void(const std::string &)> handler);

  std::string command_sync(const std::string &cmd, const std::string &arg);
  
  std::string command_sync(const std::string &cmd, const std::string &arg1, const std::string &arg2);

  std::vector<std::shared_ptr<connection>>& connections() {
    return connections_;
  }

private:
  // select one of connections and post operation to strand.
  void select_start(const std::string &message, std::function<void(const std::string &)> handler);

  int mode_;
};

} // namespace redicon

#endif // REDICON_REDICON_H
