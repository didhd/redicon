#include "redicon.h"

#include <iostream>
#include <functional>
#include <thread>

#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace redicon {

// class connection
connection::connection(boost::asio::io_context &io_context)
  : strand_(io_context),
    deadline_(io_context),
    heartbeat_timer_(io_context),
    reconnect_timer_(io_context)
{
  session_ = std::make_shared<session::element_type>(io_context);

  // size of buffer will impact performance.
  rbuf_ = std::vector<char>(4096);
}

// Function for checing deadline
void connection::check_deadline() {
  if (stopped_)
    return;

  // Check whether the deadline has passed. We compare the deadline against
  // the current time since a new asynchronous operation may have moved the
  // deadline before this actor had a chance to run.
  if (deadline_.expiry() <= boost::asio::steady_timer::clock_type::now())
  {
    // The deadline has passed. The socket is closed so that any outstanding
    // asynchronous operations are cancelled.
    // session_->close();

    // instead of closing session, we will just send error message.
    spdlog::info("Redis request timeout");

    // There is no longer an active deadline. The expiry is set to the
    // maximum time point so that the actor takes no action until a new
    // deadline is set.
    deadline_.expires_at(boost::asio::steady_timer::time_point::max());
  }

  // Put the actor back to sleep.
  deadline_.async_wait(boost::bind(&connection::check_deadline, shared_from_this()));
}

// Connects with mutliple endpoints using endpoint iterator.
void connection::do_connect(tcp::resolver::results_type::iterator endpoint_iterator) {
  if (endpoint_iterator != tcp::resolver::iterator()) {
    deadline_.expires_after(std::chrono::seconds(5));

    // Do asynchronous connect
    session_->async_connect(endpoint_iterator->endpoint(), 
        boost::asio::bind_executor(strand_, boost::bind(&connection::handle_connect, shared_from_this(), _1, endpoint_iterator)));
  }
  else {
    spdlog::error("No more endpoints to try.");
  }

  deadline_.async_wait(boost::bind(&connection::check_deadline, shared_from_this()));
}

void connection::do_reconnect(tcp::resolver::results_type::iterator endpoint_iterator) {
  session_->close();
  strand_.dispatch(boost::bind(&connection::do_connect, shared_from_this(), endpoint_iterator));
}

void connection::handle_connect(const error_code &ec, tcp::resolver::results_type::iterator endpoint_iterator) {
  if (ec) {
    spdlog::error("Connect failed: {}", ec.message());
    session_->close();

    // Try the next available endpoint.
    do_connect(++endpoint_iterator);
  }

  else if (!session_->is_open()) {
    spdlog::error("Connection timed out");

    // Try the next available endpoint.
    do_connect(++endpoint_iterator);
  }

  else {
    // There is no longer an active deadline. The expiry is set to the
    // maximum time point so that the actor takes no action until a new
    // deadline is set.
    deadline_.expires_at(boost::asio::steady_timer::time_point::max());

    std::ostringstream oss;
    
    boost::system::error_code ignored_error;
    oss << session_->local_endpoint(ignored_error);
    local_endpoint_ = oss.str();

    oss.str("");
    oss << endpoint_iterator->endpoint();
    auto remote_endpoint = oss.str();

    spdlog::info("Connected to Redis({})", remote_endpoint);

    // This has to be stranded otherwise it will do thread-unsafe works.
    strand_.post(boost::bind(&connection::start_read, shared_from_this()));
  }
}

error_code connection::do_sync_connect(tcp::resolver::results_type::iterator endpoint_iterator) {
  error_code ec;

  if (endpoint_iterator != tcp::resolver::iterator()) {
    deadline_.expires_after(std::chrono::seconds(5));  

    // Do synchronous connect
    session_->connect(endpoint_iterator->endpoint(), ec);
    if (ec) {
      spdlog::error("Connect failed: {}", ec.message());
      session_->close();

      // Try the next available endpoint.
      do_sync_connect(++endpoint_iterator);
    }
    else if (!session_->is_open()) {
      spdlog::error("Connection timed out");

      // Try the next available endpoint.
      do_sync_connect(++endpoint_iterator);
    }
    else {
      // There is no longer an active deadline. The expiry is set to the
      // maximum time point so that the actor takes no action until a new
      // deadline is set.
      deadline_.expires_at(boost::asio::steady_timer::time_point::max());

      std::ostringstream oss;
      boost::system::error_code ignored_error;
      oss << session_->local_endpoint(ignored_error);
      local_endpoint_ = oss.str();

      oss.str("");
      oss << endpoint_iterator->endpoint();
      auto remote_endpoint = oss.str();

      spdlog::info("Connected to Redis({})", remote_endpoint);
    }
  }
  else {
    spdlog::error("No more endpoints to try, closing connection");
    close();
  }

  deadline_.async_wait(boost::bind(&connection::check_deadline, shared_from_this()));
  return ec;
}

//Todo(Na): Should resolve DNS again
void connection::start_ping_sync() {
  heartbeat_timer_.expires_after(std::chrono::milliseconds(1000));
  std::string ret = do_session_sync("ping heartbeat\r\n");

  if (ret != "heartbeat") {
    spdlog::error("Redis connection closed.");

    try {
      session_->close();
    } catch (std::exception& e) {
      if (heartbeat_timer_.expiry() <= boost::asio::steady_timer::clock_type::now()){
        heartbeat_timer_.async_wait(boost::asio::bind_executor(strand_, boost::bind(&connection::start_ping_sync, shared_from_this())));
      }
    }

    try {
      session_->connect(endpoints_.begin()->endpoint());
      
      std::ostringstream oss;
      oss << endpoints_.begin()->endpoint();
      auto remote_endpoint = oss.str();

      spdlog::info("Reconnected to Redis({})", remote_endpoint);
    } catch (std::exception& e) {
      if (heartbeat_timer_.expiry() <= boost::asio::steady_timer::clock_type::now()){
        heartbeat_timer_.async_wait(boost::asio::bind_executor(strand_, boost::bind(&connection::start_ping_sync, shared_from_this())));
      }
    }
  }

  heartbeat_timer_.async_wait(boost::asio::bind_executor(strand_, boost::bind(&connection::start_ping_sync, shared_from_this())));
}

void connection::start_ping() {
  strand_.post(boost::bind(&connection::send_ping, shared_from_this()));
}

void connection::send_ping() {
  if (stopped_)
    return;

  // push ping handler, which is from client.
  handlers_.push(boost::bind(&connection::handle_heartbeat, shared_from_this(), _1));

  async_write(*session_, boost::asio::buffer("ping heartbeat\r\n", 16), 
      boost::asio::bind_executor(strand_, boost::bind(&connection::on_ping, shared_from_this(), _1, _2)));
}

void connection::on_ping(const error_code &ec, size_t bytes) {
  if (stopped_)
    return;

  if (ec) {
    spdlog::error("Cannot send ping: {}", ec.message());
    do_reconnect(endpoints_);
  }
  else {
    SPDLOG_DEBUG("<==SENT=== Redis ({},{})", local_endpoint_, boost::lexical_cast<std::string>(bytes));
    SPDLOG_DEBUG("Sent ping message");
    SPDLOG_TRACE("\n****************************************************************************************************\n"
      "* Redis <==SENT=== ({},{}) =>\n" 
      "*-------------------------------------------------------------------------------\n"
      "ping heartbeat\r\n"
      "\n****************************************************************************************************",
      local_endpoint_, boost::lexical_cast<std::string>(bytes));
  }
  heartbeat_timer_.expires_after(std::chrono::milliseconds(1000));
  heartbeat_timer_.async_wait(boost::asio::bind_executor(strand_, boost::bind(&connection::send_ping, shared_from_this())));
}

void connection::handle_heartbeat(const std::string& message) {
  if (message != "heartbeat") {
    spdlog::error("Received invalid ping message.");
    do_reconnect(endpoints_);
    if (heartbeat_timer_.expiry() <= boost::asio::steady_timer::clock_type::now()){
      heartbeat_timer_.expires_after(std::chrono::milliseconds(1000));
      heartbeat_timer_.async_wait(boost::asio::bind_executor(strand_, boost::bind(&connection::send_ping, shared_from_this())));
    }
  }
}

void connection::close() {
  stopped_ = true;
  
  boost::system::error_code ignored_error;
  session_->close(ignored_error);

  deadline_.cancel();
  heartbeat_timer_.cancel();
}

void connection::start_session(const std::string &message, std::function<void(const std::string &)> handler) {
  // This will serialize connection objects.
  // In multicore environment, this is much better than just doing do_session(message, std::move(handler));
  strand_.post(boost::bind(&connection::do_session, shared_from_this(), message, std::move(handler)));
}

void connection::do_session(const std::string &message, std::function<void(const std::string &)> handler) {
  if (stopped_)
    return;

  // handler object was passed to this function. It will be called inside strand, so that it can be thread safe.
  // This handlers will be paired later. Therefore if one of write operation fails, enqueued handler should be poped.
  // We can use lock_guard for this operation like below, but it will make entire system blocked. 
  // std::lock_guard<std::mutex> lck (mtx);
  
  handlers_.push(std::move(handler));

  // Set timeout for write operation.
  deadline_.expires_after(std::chrono::seconds(5));

  // TODO(Na): Implement scatter-gather io for this. (async_write(socket, buffers, ...))
  // using handle_write in strand is not only for better logging, but to guarantee handlers to be poped in order.
  async_write(*session_, boost::asio::buffer(message.data(), message.size()), 
      boost::asio::bind_executor(strand_, boost::bind(&connection::handle_write, shared_from_this(), _1, _2, message)));
}

std::string connection::do_session_sync(const std::string &message) {
  if (stopped_) {
    spdlog::error("Connection stopped");
    return "";
  }

  boost::asio::streambuf response;
  error_code ec;

  std::lock_guard<std::mutex> lock(mtx_);
  try {
    write(*session_, boost::asio::buffer(message.data(), message.size()));
    boost::asio::read_until(*session_, response, "\r\n", ec);
  } catch (std::exception &e) {
    return "";
  }

  if (ec) {
    spdlog::error("Cannot receive message: {}", ec.message());
    return "";
    // close();
  }

  std::istream iss(&response);
  std::string line;
  std::getline(iss, line);

  if (line.find('$') != std::string::npos) {
    // if data not exists "$-1\r\n"
    if( line.at(1) == '-' ) {
      return "";
    } 
    else {
      boost::asio::read_until(*session_, response, "\r\n", ec);
      std::getline(iss, line);
      return line.substr(0, line.size() - 1);
    }
  } else if(line.size() > 0) {
    return line.substr(0, line.size() - 1);
  }
  return line;
}

void connection::handle_write(const error_code &ec, size_t bytes, std::string message) {
  if (stopped_)
    return;

  if (ec) {
    // pops out already pushed handlers
    spdlog::error("Error sending message: {}", ec.message());
    while(!handlers_.empty()) {
      handlers_.front()("");
      handlers_.pop();
    }
    // close();
  }
  else {
    SPDLOG_DEBUG("<==SENT=== Redis ({},{})", local_endpoint_, boost::lexical_cast<std::string>(bytes));
    SPDLOG_TRACE("\n****************************************************************************************************\n"
      "* Redis <==SENT=== ({},{}) =>\n" 
      "*-------------------------------------------------------------------------------\n"
      "{}"
      "\n****************************************************************************************************", 
      local_endpoint_, boost::lexical_cast<std::string>(bytes), message);
  }
}

void connection::start_read() {
  // executing handle_read in strand is not only for better logging, but also for thread safety.
  session_->async_read_some(boost::asio::buffer(rbuf_), 
      boost::asio::bind_executor(strand_, boost::bind(&connection::handle_read, shared_from_this(),
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred)));
}

void connection::handle_read(const error_code &ec, size_t bytes_read) {
  if (stopped_)
    return;

  if (ec) {
    spdlog::error("Error receiving message: {}", ec.message());
    while(!handlers_.empty()) {
      handlers_.front()("");
      handlers_.pop(); 
    }
    // close();
  }
  else {
    // fullfilled reading operation. If there is no incoming read packet, then handle_read won't be called so that timer rings.
    deadline_.expires_at(boost::asio::steady_timer::time_point::max());

    std::string buffered_string(rbuf_.begin(), rbuf_.begin() + bytes_read); 
    if (buffered_string.empty()) {
      spdlog::error("Failed to receive data");
    }

    // Start parsing lines with line delimiter "\r\n". 
    // Brings stored_string which was unhandled in previous packet.
    std::string stored_string(stored_string_);
    stored_string_.clear();

    // Make new iss with stored_string and current packet(buffered_string).
    std::istringstream iss;
    iss.str(stored_string + buffered_string);

    // this will make pos to start from stored_string.
    int pos = -1 * stored_string.size();
    for (std::string line; std::getline(iss, line); pos = pos + line.size() + 1) {

      // if line isn't empty,
      if (!line.empty()) {
        // if line doesn't ends with '\r', (Usually last line of packet)
        if (line.back() != '\r') {
          stored_string_.insert(stored_string_.begin(), buffered_string.begin() + pos, buffered_string.end());
          break;
        }
        // every line below ends with '\r'
        if (line.find('$') != std::string::npos) {
          // if data not exists "$-1\r"
          if( line.at(1) == '-' ) {
            if( handlers_.empty() == false ) {
              spdlog::debug("Redis data not exists");
              handlers_.front()("");
              handlers_.pop();
            } else {
              spdlog::error("Cannot execute empty handler");
              return;
            }
          }

          // found '$' in line, but normal lines "$3\r" can be ignored.
        } else {
          // not found '$' in line
          // assert(strand_.running_in_this_thread());
          if( handlers_.empty() == false ) {
            // trim '\r' from the string.
            handlers_.front()(line.substr(0, line.size() - 1));
            SPDLOG_DEBUG("===RECV==> Redis ({},{})", local_endpoint_, boost::lexical_cast<std::string>(line.size() -1));
            SPDLOG_TRACE("\n****************************************************************************************************\n" 
              "* Redis ===RECV==> ({},{}) =>\n" 
              "*-------------------------------------------------------------------------------\n"
              "{}"
              "\n****************************************************************************************************",
              local_endpoint_, boost::lexical_cast<std::string>(line.size() -1), line.substr(0, line.size() - 1));
            handlers_.pop();
          } else {
            spdlog::error("Cannot execute empty handler");
            return;
          }
        }
      } else { 
        // case 1: empty line, keep reading.
        // "...\n1234\r \n1235..." => "stored: none" "next read: \n1235..." => "next line: empty"

        // case 2: redis returns empty string which is "$0\r\n\r\n"
        // "stored string with "

        SPDLOG_DEBUG("Empty line, keep reading");
      }
    } // end of for loop

    start_read();
  }
}

//------------------------------------------------------------------------------

// class client
client::client()
  : io_context_(1),
    strand_(io_context_) {}

client::client(boost::asio::io_context &io_context)
  : strand_(io_context) {}

// TODO(Na): Use asynchronous connect.
void client::connect(const std::string &hostname, const int &port) {
  error_code ec;

  // Resolve hostname.
  tcp::resolver resolver(strand_.get_io_context());
  tcp::resolver::results_type endpoints = resolver.resolve(hostname, std::to_string(port));

  // create connection and insert it into connections_ vector.
  auto con = std::make_shared<connection>(strand_.get_io_context());
  connections_.push_back(con);

  // give connections endpoints information.
  con->endpoints() = endpoints;

  ec = con->do_sync_connect(endpoints.begin());
  if (!ec) {
    con->start_read();
    con->start_ping();
  }
}

void client::connect(const std::string &hostname, const int &port, size_t count) { 
  for (size_t i = 0; i < count; i++)
    connect(hostname, port);
}

// Todo(Na): Create synchronous client class.
void client::connect_sync(const std::string &hostname, const int &port) {
  error_code ec;

  tcp::resolver resolver(strand_.get_io_context());
  tcp::resolver::results_type endpoints = resolver.resolve(hostname, std::to_string(port));

  auto con = std::make_shared<connection>(strand_.get_io_context());
  connections_.push_back(con);

  con->endpoints() = endpoints;
  ec = con->do_sync_connect(endpoints.begin());
  if (!ec)
    con->start_ping_sync();
}

void client::connect_sync(const std::string &hostname, const int &port, size_t count) { 
  for (size_t i = 0; i < count; i++)
    connect_sync(hostname, port);
}

void client::run() {
  strand_.get_io_context().run();
}

void client::run_detached()
{
  // The io_context::run() call will block until all asynchronous operations
  // have finished. While the server is running, there is always at least one
  // asynchronous operation outstanding: the asynchronous accept call waiting
  // for new incoming connections.
  // strand_.get_io_context().run();

  // This will assign one thread for each client. 
  // TODO(Na): Should be changed to workers.
  std::thread t([this] {
    strand_.get_io_context().run();
  });
  t.detach();
}

void client::run_detached(int concurrency_hint) {
  std::vector<std::thread> v;
  v.reserve(concurrency_hint);
  for(auto i = concurrency_hint; i > 0; --i) {
    std::thread t([this] {
      strand_.get_io_context().run();
    });
    t.detach();
  }
}

void client::stop() {
  for (auto con : connections_)
    con->close();
}

void client::command(const std::string &cmd, std::function<void(const std::string &)> handler) {
  std::string message = cmd + "\r\n";

  strand_.post(boost::bind(&client::select_start, this, std::move(message), std::move(handler)));
}

void client::command(const std::string &cmd, const std::string &arg, std::function<void(const std::string &)> handler){
  std::string message = cmd + " " + arg + "\r\n";
  
  // TODO(Na): Fix usage of "this" and bad weak ptr when using shared_from_this()
  strand_.post(boost::bind(&client::select_start, this, std::move(message), std::move(handler)));
}

void client::command(const std::string &cmd, const std::string &arg1, const std::string &arg2, std::function<void(const std::string &)> handler){
  std::string message = cmd + " " + arg1 + " " + arg2 + "\r\n";

  strand_.post(boost::bind(&client::select_start, this, std::move(message), std::move(handler)));
}

void client::command(const std::initializer_list<std::string> &args, std::function<void(const std::string &)> handler) {
  std::string message;
  for(auto arg : args) {
    message.append(" " + arg);
  }
  message.append("\r\n");

  strand_.post(boost::bind(&client::select_start, this, std::move(message), std::move(handler)));
}

std::string client::command_sync(const std::string &cmd, const std::string &arg) {
  std::string message = cmd + " " + arg + "\r\n";

  std::string reply = "";
  auto sync_handler = [&reply](const std::string &repl) {
    reply = repl;
  };

  if (connections_.size() == 0) {
    spdlog::error("No connection available");
    return "";
  }

  auto con = selector_(connections_);
  return con->do_session_sync(std::move(message));
}

std::string client::command_sync(const std::string &cmd, const std::string &arg1, const std::string &arg2) {
  std::string message = cmd + " " + arg1 + " " + arg2 + "\r\n";

  std::string reply = "";
  auto sync_handler = [&reply](const std::string &repl) {
    reply = repl;
  };

  if (connections_.size() == 0) {
    spdlog::error("No connection available");
    return "";
  }

  auto con = selector_(connections_);
  return con->do_session_sync(std::move(message));
}

// TODO(Na): Determine which part is thread local.
// This is important, because it will impact performance a lot.
void client::select_start(const std::string &message, std::function<void(const std::string &)> handler) {
  if (connections_.size() == 0) {
    spdlog::error("No connection available");
    handler("");
    return;
  }

  auto con = selector_(connections_);
  con->start_session(std::move(message), std::move(handler));
}

} // namespace redicon
