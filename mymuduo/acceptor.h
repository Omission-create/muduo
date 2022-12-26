#pragma once
#include "channel.h"
#include "socket.h"
#include <functional>

class EventLoop;

class Acceptor
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = cb;
    }

    bool listening() const { return listenning_; }
    void listen();

private:
    void handleRead();

    EventLoop *loop_; // baseloop
    Socket accept_socket_;
    Channel accept_channel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};