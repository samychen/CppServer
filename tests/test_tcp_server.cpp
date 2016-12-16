//
// Created by Ivan Shynkarenka on 16.12.2016.
//

#include "catch.hpp"

#include "server/asio/tcp_client.h"
#include "server/asio/tcp_server.h"
#include "server/asio/tcp_session.h"

using namespace CppCommon;
using namespace CppServer::Asio;

class EchoService : public Service
{
public:
    bool thread_initialize;
    bool thread_cleanup;
    bool starting;
    bool started;
    bool stopping;
    bool stopped;
    bool idle;
    bool error;

    explicit EchoService()
        : thread_initialize(false),
          thread_cleanup(false),
          starting(false),
          started(false),
          stopping(false),
          stopped(false),
          idle(false),
          error(false)
    {
    }

protected:
    void onThreadInitialize() override { thread_initialize = true; }
    void onThreadCleanup() override { thread_cleanup = true; }
    void onStarting() override { starting = true; }
    void onStarted() override { started = true; }
    void onStopping() override { stopping = true; }
    void onStopped() override { stopped = true; }
    void onIdle() override { idle = true; }
    void onError(int error, const std::string& category, const std::string& message) override { error = true; }
};

class EchoClient : public TCPClient
{
public:
    bool connected;
    bool disconnected;
    size_t received;
    size_t sent;
    bool error;

    explicit EchoClient(EchoService& service, const std::string& address, int port)
        : TCPClient(service, address, port),
          connected(false),
          disconnected(false),
          received(0),
          sent(0),
          error(false)
    {
    }

protected:
    void onConnected() override { connected = true; }
    void onDisconnected() override { disconnected = true; Thread::Sleep(1); Connect(); }
    size_t onReceived(const void* buffer, size_t size) override { received += size; return size; }
    void onSent(size_t sent, size_t pending) override { this->sent += sent; }
    void onError(int error, const std::string& category, const std::string& message) override { error = true; }
};

class EchoServer;

class EchoSession : public TCPSession<EchoServer, EchoSession>
{
public:
    bool connected;
    bool disconnected;
    size_t received;
    size_t sent;
    bool error;

    explicit EchoSession(EchoServer& server, const UUID& uuid, asio::ip::tcp::socket socket)
        : TCPSession<EchoServer, EchoSession>(server, uuid, std::move(socket)),
          connected(false),
          disconnected(false),
          received(0),
          sent(0),
          error(false)
    {
    }

protected:
    void onConnected() override { connected = true; }
    void onDisconnected() override { disconnected = true; }
    size_t onReceived(const void* buffer, size_t size) override { Send(buffer, size); received += size; return size; }
    void onSent(size_t sent, size_t pending) override { this->sent += sent; }
    void onError(int error, const std::string& category, const std::string& message) override { error = true; }
};

class EchoServer : public TCPServer<EchoServer, EchoSession>
{
public:
    bool connected;
    bool disconnected;
    size_t received;
    size_t sent;
    bool error;

    explicit EchoServer(EchoService& service, InternetProtocol protocol, int port)
        : TCPServer<EchoServer, EchoSession>(service, protocol, port),
          connected(false),
          disconnected(false),
          received(0),
          sent(0),
          error(false)
    {
    }

protected:
    void onConnected(std::shared_ptr<EchoSession> session) override { connected = true; }
    void onDisconnected(std::shared_ptr<EchoSession> session) override { disconnected = true; received += session->received; sent += session->sent; }
    void onError(int error, const std::string& category, const std::string& message) override { error = true; }
};

TEST_CASE("TCP server & client", "[CppServer]")
{
    EchoService service;
    service.Start();

    EchoServer server(service, InternetProtocol::IPv4, 1234);
    server.Accept();

    EchoClient client(service, "127.0.0.1", 1234);
    client.Connect();

    // Wait for a while...
    Thread::Sleep(5000);

    client.Send("test", 4);

    // Wait for a while...
    Thread::Sleep(5000);

    client.Disconnect();

    // Wait for a while...
    Thread::Sleep(5000);

    service.Stop();

    // Check the service state
    REQUIRE(service.thread_initialize);
    REQUIRE(service.thread_cleanup);
    REQUIRE(service.starting);
    REQUIRE(service.started);
    REQUIRE(service.stopping);
    REQUIRE(service.stopped);
    REQUIRE(service.idle);
    REQUIRE(!service.error);

    // Check the client state
    REQUIRE(client.connected);
    REQUIRE(client.disconnected);
    REQUIRE(client.received == 4);
    REQUIRE(client.sent == 4);
    REQUIRE(!client.error);

    // Check the server state
    REQUIRE(server.connected);
    REQUIRE(server.disconnected);
    REQUIRE(server.received == 4);
    REQUIRE(server.sent == 4);
    REQUIRE(!server.error);
}


TEST_CASE("TCP server broadcast ", "[CppServer]")
{
    EchoService service;
    service.Start();

    EchoServer server(service, InternetProtocol::IPv4, 1234);
    server.Accept();

    EchoClient client1(service, "127.0.0.1", 1234);
    client1.Connect();

    EchoClient client2(service, "127.0.0.1", 1234);
    client2.Connect();

    EchoClient client3(service, "127.0.0.1", 1234);
    client3.Connect();

    // Wait for a while...
    Thread::Sleep(5000);

    server.Broadcast("test", 4);

    // Wait for a while...
    Thread::Sleep(5000);

    server.DisconnectAll();

    // Wait for a while...
    Thread::Sleep(5000);

    service.Stop();

    // Check the service state
    REQUIRE(service.thread_initialize);
    REQUIRE(service.thread_cleanup);
    REQUIRE(service.starting);
    REQUIRE(service.started);
    REQUIRE(service.stopping);
    REQUIRE(service.stopped);
    REQUIRE(service.idle);
    REQUIRE(!service.error);

    // Check the client state
    REQUIRE(client1.received == 4);
    REQUIRE(client1.sent == 0);
    REQUIRE(!client1.error);
    REQUIRE(client2.received == 4);
    REQUIRE(client2.sent == 0);
    REQUIRE(!client2.error);
    REQUIRE(client3.received == 4);
    REQUIRE(client3.sent == 0);
    REQUIRE(!client3.error);

    // Check the server state
    REQUIRE(server.received == 0);
    REQUIRE(server.sent == 12);
    REQUIRE(!server.error);
}