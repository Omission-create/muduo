#include "acceptor.h"
#include "channel.h"
#include <sys/socket.h>
#include <sys/types.h>
#include "logger.h"
#include "inetaddress.h"
#include <unistd.h>
#include <errno.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d createNonblocking failed!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),
      accept_socket_(createNonblocking()),
      accept_channel_(loop, accept_socket_.fd()),
      listenning_(false)
{
    accept_socket_.setReuseAddr(true);
    accept_socket_.setReusePort(true);
    accept_socket_.bindAddress(listenAddr);
    // 接收连接后执行回调，分发给subloop
    accept_channel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    accept_channel_.disableAll();
    accept_channel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    accept_socket_.listen();         // listen
    accept_channel_.enableReading(); //=> update() =>Poller::updateChannel()
}

// listenfd用新用户连接后分发
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = accept_socket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr); // 轮询找到subloop,分发当前channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept_socket_.accept failed!\n ", __FILE__, __FUNCTION__, __LINE__);
    }
}