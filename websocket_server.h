#pragma once

#include <websocketpp/config/asio.hpp>

#include <websocketpp/server.hpp>

#include <iostream>
#include <set>

#include <websocketpp/common/thread.hpp>
#include "message_queue.h"


using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using websocketpp::lib::thread;
using websocketpp::lib::mutex;
using websocketpp::lib::lock_guard;
using websocketpp::lib::unique_lock;
using websocketpp::lib::condition_variable;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

enum action_type {
  SUBSCRIBE,
  TLS_SUBSCRIBE,
  UNSUBSCRIBE,
  MESSAGE,
  EXIT
};

class condition_mutex {
public:
  bool wait(int time) {
    std::unique_lock<std::mutex> lock(lock_);
    ready_ = false;
    return cv_.wait_for(lock, std::chrono::milliseconds(time), [&] {return ready_; });
  }
  void notify(int reason = 0) {
    std::lock_guard<std::mutex> lk(lock_);
    ready_ = true;
    reason_ = reason;
    cv_.notify_all();
  }
  bool ready() const { return ready_; }
  int notify_reason() const { return reason_; }
protected:
  int reason_ = 0;
  std::condition_variable cv_;
  std::mutex lock_;
  bool ready_ = false;
};

typedef websocketpp::server<websocketpp::config::asio> server_plain;
typedef websocketpp::server<websocketpp::config::asio_tls> server_tls;
class WebsocketServer {
public:
  typedef server_plain::message_ptr message_ptr;

  struct action {
  action(action_type t, connection_hdl h) : type(t), hdl(h) {}
  action(action_type t, connection_hdl h, message_ptr m)
    : type(t), hdl(h), msg(m) {}

  action_type type;
  websocketpp::connection_hdl hdl;
  message_ptr msg;
};
  WebsocketServer();
  void Listen(int port,int port_tls=0);

  bool Send(void* data, int len,connection_hdl hdl);
  bool Send(const std::string& text,connection_hdl hdl);

  void Broadcast(const std::string& text);
  void Broadcast(void* data, int len);
  virtual void OnReceive(connection_hdl hdl, const std::string& message) = 0;
  virtual void OnClose(connection_hdl) = 0;
protected:
  void run(uint16_t port,uint16_t port_tls);

  void on_open(connection_hdl hdl);
  void on_open_tls(connection_hdl hdl);

  void on_close(connection_hdl hdl);

  void on_message(connection_hdl hdl, server_plain::message_ptr msg);

  void on_pong_timeout(connection_hdl hdl, std::string s);

  void process_messages();

  void loop_ping();

  void wait_exit_message();

  server_plain m_server_plain;
  server_tls m_server_tls;

protected:
  typedef std::set<connection_hdl, std::owner_less<connection_hdl> > con_list;

  bool m_exit_signal;
  con_list m_con_list_plain;
  con_list m_con_list_tls;
  std::queue<action> m_actions;

  mutex m_action_lock;
  mutex m_connection_lock;
  condition_variable m_action_cond;

  condition_mutex m_mutex_exit;
  MessageQueue m_message_queue;
  boost::asio::io_service m_ios;
};
