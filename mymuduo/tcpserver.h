#pragma once

#include "eventloop.h"
#include "eventloop_threadpool.h"
#include "acceptor.h"
#include "inetaddress.h"
#include "noncopyable.h"
#include "tcp_connection.h"
#include "buffer.h"
#include <functional>
#include <string>
#include "callbacks.h"
#include <atomic>
#include <unordered_map>

class TcpServer
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop, const InetAddress &listenaddr, const std::string &nameArg, Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback cb) { writeCompleteCallback_ = cb; }

    // 设置底层subloop的个数
    void setThreadNum(int numThreads);

    // 开启服务器监听
    void start();

    // 有一个新的客户端连接，acceptor会执行这个回调操作

private:
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    // 有一个新的客户端连接，acceptor会执行这个回调操作
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    EventLoop *loop_; // acceptor loop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;              // 运行mainloop 接收连接
    std::unique_ptr<EventLoopThreadPool> threadpool_; // one loop one thread
    ConnectionCallback connectionCallback_;           // 接收新连接时的回调
    MessageCallback messageCallback_;                 // 接收消息的回调
    WriteCompleteCallback writeCompleteCallback_;     // 消息发送完成回调
    ThreadInitCallback threadInitCallback_;

    std::atomic_int started_;
    int nextConnId_;
    ConnectionMap connections_; // 保存所有的连接
};