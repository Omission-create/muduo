#pragma once
#include "noncopyable.h"
#include "inetaddress.h"
#include "callbacks.h"
#include <memory>
#include <string>
#include <atomic>
#include "buffer.h"
#include "timestamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * @brief TcpServer => Acceptor => 有一个新用户链接，通过accept函数拿到connfd
 *      => TcpConnection 设置回调 => channel => channel的回调操作
 */
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop, const std::string &name, int sockfd, const InetAddress &localaddr, const InetAddress &peeraddr);
    ~TcpConnection();

    EventLoop *getloop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localaddr() { return localaddr_; }
    const InetAddress &peeraddr() { return peeraddr_; }

    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string &buf);
    // 关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }

    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyde();

private:
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const void *message, size_t len);

    void shutdownInLoop();

    enum State
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting // 当outputBuffer还有数据每发送完就shutdown
    };

    void setState(State state) { state_ = state; }

    EventLoop *loop_; // 这里不是baseloop，因为TcpConnection都是在subloop中管理的

    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localaddr_;
    const InetAddress peeraddr_;

    ConnectionCallback connectionCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;
    MessageCallback messageCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;

    size_t highWaterMark_;
    Buffer inputBuffer_;  // 读fd
    Buffer outputBuffer_; // 写fd
};