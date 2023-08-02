#include "tcp_connection.h"
#include "logger.h"
#include "socket.h"
#include "channel.h"
#include "eventloop.h"
#include <functional>
#include <string>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%d:%s CheckLoopNotNull loop==nullptr", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &name, int sockfd, const InetAddress &localaddr, const InetAddress &peeraddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(name),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      peeraddr_(localaddr),
      localaddr_(localaddr),
      highWaterMark_(64 * 1024 * 1024)
{
    // 下面给channel设置相应的回调函数，poller给channel通知感兴趣事件，channel调用回调函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}
TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", name_.c_str(), socket_->fd(), int(state_));
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno); // LT模式
    if (n > 0)
    {
        // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        // shared_from_this 返回当前对象的智能指针
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    // LT模式会一直上报fd可写 直至buffer写空时disableWriting
    if (channel_->isWriting()) // 是否可写
    {
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n); // 复位
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    // 唤醒loop_对应的thread线程，执行回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing\n", socket_->fd());
    }
}

// poller=>channel::closeCallback=>TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); // onConnection包含关闭连接的部分
    closeCallback_(connPtr);      // 关闭连接的回调(TcpServer注册的)
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}

// 第一次send没有发送完，会把数据放到buffer，并注册epollout事件，调用handlewrite
// 发送数据：应用写的块，内核发送慢 需要将数据暂时缓存到缓冲区
void TcpConnection::sendInLoop(const void *message, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 之前调用过该connection的shutdown，不能发送
    if (state_ == kDisconnected)
    {
        LOG_ERROR("TcpConnection::sendInLoop discoonected");
    }

    // 表示channel第一次开始写数据【此时还没有给channel注册EPOLLOUT事件】，而且缓冲区没有待发送的数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), message, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                // 这里数据发送完成，就不用再给channel注册EPOLLOUT事件，就不会调用handleWrite方法
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote<0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK) // 非阻塞正常的返回
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE RESET
                {
                    faultError = true;
                }
            }
        }
    }

    // 说明当前write没有完全发送出去，剩余数据需要保存到缓冲区中，然后给channel
    // 注册epollout事件，poller发现tcp的发送缓冲区有空间，【LT模式不断】会通知相应的channel，调用writeCallback回调方法
    // 也就是调用TcpConnection::handleWrite方法，把发送缓冲的数据全部发送完成
    if (!faultError && remaining > 0)
    {
        // 目前缓冲区待发送的数据的长度
        size_t oldLen = outputBuffer_.readableBytes();

        // 已经超过高水位
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char *)message + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting(); // 里面包含了注册事件
        }
    }
}
void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting())
    {
        // 说明当前outputBuffer的数据已经全部完成
        socket_->shutdownWrite(); // 关闭写端，触发EPOLLHUP
    }
}

// 关闭连接
void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    // 开始只对读感兴趣
    channel_->enableReading();

    // 新连接建立。执行回调
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyde()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // 把channel_感兴趣的事件全部del
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // 把channel从Poller中删除
}