#include "socket.h"
#include "inetaddress.h"
#include "logger.h"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <string.h>
Socket::~Socket()
{
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr)
{
    if (0 != ::bind(sockfd_, (sockaddr *)localaddr.getSockaddr(), sizeof(sockaddr)))
    {
        LOG_FATAL("Socket::bindAddress %d failed!\n", sockfd_);
    }
}

void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL("Socket::listen %d failed!\n", sockfd_);
    }
}
int Socket::accept(InetAddress *peeraddr)
{
    /*
     *   1.accept函数的参数不合法（len必须初始化）
     *   2.对返回的connfd没有设置非阻塞
     */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t len = sizeof(addr);
    int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0)
    {
        peeraddr->setSocketAddr(addr);

        // // 设置成非阻塞
        // int flags = ::fcntl(connfd, F_GETFL, 0);
        // flags |= O_NONBLOCK;
        // int ret = ::fcntl(connfd, F_SETFL, flags);

        // //
        // flags = ::fcntl(connfd, F_GETFD, 0);
        // flags |= FD_CLOEXEC;
        // ret = ::fcntl(connfd, F_SETFD, flags);
    }

    return connfd;
}

void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_FATAL("Socket::shutdownWrite %d failed!\n", sockfd_);
    }
}
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}
void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}
void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}