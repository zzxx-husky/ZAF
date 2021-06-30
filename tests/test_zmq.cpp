#include <thread>

#include "gtest/gtest.h"
#include "glog/logging.h"

#include "zmq.hpp"

namespace zaf {
GTEST_TEST(ZMQ, Router2Router1x1) {
  zmq::context_t ctx;
  std::string client_identifier = "client";
  std::string server_identifier = "server";

  zmq::socket_t server_sock(ctx, zmq::socket_type::router);
  server_sock.set(zmq::sockopt::routing_id, zmq::buffer(server_identifier));
  server_sock.bind("inproc://server");

  zmq::socket_t client_sock(ctx, zmq::socket_type::router);
  client_sock.set(zmq::sockopt::routing_id, zmq::buffer(client_identifier));

  std::thread server([&]() {
    auto& sock = server_sock;

    for (int i = 0; i < 10; i++) { // receive Hello and reply World
      zmq::message_t client_id;
      EXPECT_TRUE(sock.recv(client_id));
      EXPECT_EQ(client_identifier, std::string((char*)client_id.data(), client_id.size()));
      zmq::message_t msg;
      EXPECT_TRUE(sock.recv(msg));
      EXPECT_EQ("Hello", std::string((char*)msg.data(), msg.size()));

      sock.send(client_id, zmq::send_flags::sndmore);
      sock.send(zmq::str_buffer("World"));
    }

    { // actively send from server to client
      sock.send(zmq::buffer(client_identifier), zmq::send_flags::sndmore);
      sock.send(zmq::str_buffer("Hello World"));
    }

    LOG(INFO) << "Server terminates.";
    sock.close();
  });

  std::thread client([&]() {
    auto& sock = client_sock;
    sock.connect("inproc://server");

    for (int i = 0; i < 10; i++) { // send Hello
      sock.send(zmq::buffer(server_identifier), zmq::send_flags::sndmore);
      sock.send(zmq::str_buffer("Hello"));

      zmq::message_t server_id;
      EXPECT_TRUE(sock.recv(server_id));
      EXPECT_EQ(server_identifier, std::string((char*)server_id.data(), server_id.size()));
      zmq::message_t msg;
      EXPECT_TRUE(sock.recv(msg));
      EXPECT_EQ("World", std::string((char*)msg.data(), msg.size()));
    }

    LOG(INFO) << "C->S->C is good.";

    {
      zmq::message_t server_id;
      EXPECT_TRUE(sock.recv(server_id));
      EXPECT_EQ(server_identifier, std::string((char*)server_id.data(), server_id.size()));
      zmq::message_t msg;
      EXPECT_TRUE(sock.recv(msg));
      EXPECT_EQ("Hello World", std::string((char*)msg.data(), msg.size()));
    }

    LOG(INFO) << "S->C is good.";

    LOG(INFO) << "Client terminates.";
    sock.close();
  });

  server.join();
  client.join();
}

GTEST_TEST(ZMQ, Router2Router1x1ClientNoRoutingId) {
  zmq::context_t ctx;
  std::string server_identifier = "server";

  zmq::socket_t server_sock(ctx, zmq::socket_type::router);
  server_sock.set(zmq::sockopt::routing_id, zmq::buffer(server_identifier));
  server_sock.bind("inproc://server");

  zmq::socket_t client_sock(ctx, zmq::socket_type::router);

  std::thread server([&]() {
    auto& sock = server_sock;

    for (int i = 0; i < 10; i++) { // receive Hello and reply World
      zmq::message_t client_id;
      EXPECT_TRUE(sock.recv(client_id));
      zmq::message_t msg;
      EXPECT_TRUE(sock.recv(msg));
      EXPECT_EQ("Hello World", std::string((char*)msg.data(), msg.size()));
    }

    LOG(INFO) << "Server terminates.";
    sock.close();
  });

  std::thread client([&]() {
    auto& sock = client_sock;
    sock.connect("inproc://server");

    for (int i = 0; i < 10; i++) { // send Hello
      sock.send(zmq::buffer(server_identifier), zmq::send_flags::sndmore);
      sock.send(zmq::str_buffer("Hello World"));
    }

    LOG(INFO) << "C->S is good.";
    LOG(INFO) << "Client terminates.";
    sock.close();
  });

  server.join();
  client.join();
}

GTEST_TEST(ZMQ, Router2RouterTwoServers) {
  zmq::context_t ctx;

  zmq::socket_t server1(ctx, zmq::socket_type::router);
  server1.set(zmq::sockopt::routing_id, zmq::str_buffer("server1"));
  server1.bind("inproc://server1");

  zmq::socket_t server2(ctx, zmq::socket_type::router);
  server2.set(zmq::sockopt::routing_id, zmq::str_buffer("server2"));
  server2.bind("inproc://server2");

  server1.connect("inproc://server2");
  server2.connect("inproc://server1");

  std::thread server1_thread([&]() {
    auto& sock = server1;

    for (int i = 0; i < 10; i++) { // receive Hello and reply World
      sock.send(zmq::str_buffer("server2"), zmq::send_flags::sndmore);
      sock.send(zmq::str_buffer("Hello"));

      zmq::message_t sender;
      EXPECT_TRUE(sock.recv(sender));
      EXPECT_EQ("server2", std::string((char*)sender.data(), sender.size()));
      zmq::message_t msg;
      EXPECT_TRUE(sock.recv(msg));
      EXPECT_EQ("World", std::string((char*)msg.data(), msg.size()));
    }

    LOG(INFO) << "Server 1 terminates.";
    sock.close();
  });

  std::thread server2_thread([&]() {
    auto& sock = server2;

    for (int i = 0; i < 10; i++) { // receive Hello and reply World
      sock.send(zmq::str_buffer("server1"), zmq::send_flags::sndmore);
      sock.send(zmq::str_buffer("World"));

      zmq::message_t sender;
      EXPECT_TRUE(sock.recv(sender));
      EXPECT_EQ("server1", std::string((char*)sender.data(), sender.size()));
      zmq::message_t msg;
      EXPECT_TRUE(sock.recv(msg));
      EXPECT_EQ("Hello", std::string((char*)msg.data(), msg.size()));
    }

    LOG(INFO) << "Server 2 terminates.";
    sock.close();
  });

  server1_thread.join();
  server2_thread.join();
}

GTEST_TEST(ZMQ, Router2Router4x4) {
  zmq::context_t ctx;
  std::string client_identifier = "client";
  std::string server_identifier = "server";
  int C = 4;
  int S = 4;

  std::vector<zmq::socket_t> server_socks;
  for (int n = 0; n < S; n++) {
    server_socks.emplace_back(zmq::socket_t(ctx, zmq::socket_type::router));
    auto& sock = server_socks.back();
    auto self_id = server_identifier + std::to_string(n);
    sock.set(zmq::sockopt::routing_id, zmq::buffer(self_id));
    sock.bind("inproc://server" + std::to_string(n));
  }

  std::vector<zmq::socket_t> client_socks;
  for (int n = 0; n < C; n++) {
    client_socks.emplace_back(zmq::socket_t(ctx, zmq::socket_type::router));
    auto& sock = client_socks.back();
    auto self_id = client_identifier + std::to_string(n);
    sock.set(zmq::sockopt::routing_id, zmq::buffer(self_id));
  }

  std::vector<std::thread> servers;
  for (int n = 0; n < S; n++) {
    servers.emplace_back([&, n]() {
      // server0, server1, ...
      auto self_id = server_identifier + std::to_string(n);
      auto& sock = server_socks[n];

      for (int i = 0; i < 3*C; i++) { // receive Hello and reply World
        zmq::message_t client_id;
        EXPECT_TRUE(sock.recv(client_id));
        EXPECT_EQ(client_identifier, std::string((char*)client_id.data(), client_identifier.size()));
        zmq::message_t msg;
        EXPECT_TRUE(sock.recv(msg));
        EXPECT_EQ("Hello", std::string((char*)msg.data(), 5));

        sock.send(client_id, zmq::send_flags::sndmore);
        auto reply = "World" + std::string((char*)msg.data() + 5, msg.size() - 5);
        sock.send(zmq::buffer(reply));
        // LOG(INFO) << self_id << " sends " << reply << " to " << std::string((char*)client_id.data(), client_id.size());
      }

      for (int x = 0; x < 4; x++) { // actively send from server to client
        auto cid = client_identifier + std::to_string(x);
        sock.send(zmq::buffer(cid), zmq::send_flags::sndmore);
        sock.send(zmq::str_buffer("Hello World"));
        // LOG(INFO) << self_id << " sends Hello World";
      }

      LOG(INFO) << "Server " << n << " terminates.";
      sock.close();
    });
  }

  std::vector<std::thread> clients;
  for (int n = 0; n < C; n++) {
    clients.emplace_back([&, n]() {
      // client0, client1, ...
      auto self_id = client_identifier + std::to_string(n);
      auto& sock = client_socks[n];
      // connect to server0, server1, ...
      for (int x = 0; x < 4; x++) {
        sock.connect("inproc://server" + std::to_string(x));
      }

      int cnt_hello_world = 0;
      // send Hello0, Hello1, ... to all servers
      for (int i = 0; i < 3*S; i++) { // send Hello
        auto sid = server_identifier + std::to_string(i % S);
        sock.send(zmq::buffer(sid), zmq::send_flags::sndmore);
        auto hello = "Hello" + std::to_string(i / S);
        sock.send(zmq::buffer(hello));
        // LOG(INFO) << self_id << " sends " << hello << " to " << sid;

        while (true) {
          zmq::message_t server_id;
          EXPECT_TRUE(sock.recv(server_id));
          if (sid == std::string((char*)server_id.data(), server_id.size())) {
            break;
          }
          EXPECT_EQ(server_identifier, std::string((char*)server_id.data(), server_identifier.size()));
          zmq::message_t msg;
          EXPECT_TRUE(sock.recv(msg));
          EXPECT_EQ("Hello World", std::string((char*)msg.data(), msg.size()));
          ++cnt_hello_world;
          EXPECT_LE(cnt_hello_world, 4);
        }
        // send Hellox -> receive Worldx
        zmq::message_t msg;
        EXPECT_TRUE(sock.recv(msg));
        EXPECT_EQ("World" + std::to_string(i / S), std::string((char*)msg.data(), msg.size()));
      }

      LOG(INFO) << self_id << ": C->S->C is good.";

      for (; cnt_hello_world < 4; cnt_hello_world++) {
        zmq::message_t server_id;
        EXPECT_TRUE(sock.recv(server_id));
        EXPECT_EQ(server_identifier, std::string((char*)server_id.data(), server_identifier.size()));
        zmq::message_t msg;
        EXPECT_TRUE(sock.recv(msg));
        EXPECT_EQ("Hello World", std::string((char*)msg.data(), msg.size()));
      }

      LOG(INFO) << self_id << ": S->C is good.";

      LOG(INFO) << "Client " << n << " terminates.";
      sock.close();
    });
  }

  for (auto& server : servers) {
    server.join();
  }
  server_socks.clear();
  for (auto& client : clients) {
    client.join();
  }
  client_socks.clear();
}

// GTEST_TEST(ZMQ, SelfLoop) {
//   zmq::context_t ctx;
// 
//   zmq::socket_t server(ctx, zmq::socket_type::router);
//   server.set(zmq::sockopt::routing_id, zmq::str_buffer("server"));
//   server.bind("inproc://server");
// 
//   // server.connect("inproc://server");
// 
//   std::thread server_thread([&]() {
//     auto& sock = server;
// 
//     for (int i = 0; i < 10; i++) { // receive Hello and reply World
//       sock.send(zmq::str_buffer("server"), zmq::send_flags::sndmore);
//       sock.send(zmq::str_buffer("Hello World"));
// 
//       zmq::message_t sender;
//       EXPECT_TRUE(sock.recv(sender));
//       EXPECT_EQ("server", std::string((char*)sender.data(), sender.size()));
//       zmq::message_t msg;
//       EXPECT_TRUE(sock.recv(msg));
//       EXPECT_EQ("Hello World", std::string((char*)msg.data(), msg.size()));
//     }
// 
//     LOG(INFO) << "Server terminates.";
//     sock.close();
//   });
// 
//   server_thread.join();
// }
} // namespace zaf
