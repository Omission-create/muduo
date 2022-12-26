#include "tcpserver.h"
#include "logger.h"
#include <functional>
#include <strings.h>
#include "tcp_connection.h"

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%d:%s CheckLoopNotNull loop==nullptr", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenaddr, const std::string &nameArg, Option option)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenaddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenaddr, option == kReusePort)),
      threadpool_(new EventLoopThreadPool(loop, name_)),
      connectionCallback_(),
      messageCallback_(),
      nextConnId_(),
      started_(0)
{
    // 给listenfd注册回调，当有新用户连接受调用回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}
TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second); // conn局部智能指针对象，出右括号自动释放new出来的TcpConnection资源
        item.second.reset();
        conn->getloop()->runInLoop(std::bind(&TcpConnection::connectDestroyde, conn));
    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadpool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TcpServer::start()
{
    if (started_++ == 0) // 防止一个tcpserver对象被start多次
    {
        threadpool_->start(threadInitCallback_); // 底层启动线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getloop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyde, conn));
}

/**
 * @brief 根据round robin选择一个subloop，唤醒subloop,
 *      把当前connfd封装成channel分发给subloop
 * @param sockfd
 * @param peerAddr
 */
// Acceptor::handleRead==>Tcpserver::newConnection
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    EventLoop *ioLoop = threadpool_->getNextLoop();
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection %s - new connection %s from %s\n", name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机ip地址和端口号
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getlocalAddr");
    }
    InetAddress localAddr(local);

    // 根据链接成功的sockfd,创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));

    connections_[connName] = conn;

    // 下面所有的回调用户设置给TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调函数
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置如何关闭连接 conn->shutdown
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 直接调用TcpConnection::connectEstalished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}