#pragma once

#include "noncopyable.h"
#include <functional>
#include "timestamp.h"
#include <memory>

class EventLoop;

/**
 * @brief Channel 理解为通道，封装sockfd和event，如EPOLLIN、EPOLLOUT事件
 * 还绑定了poller返回具体事件
 *
 */
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>; // 替代typedef
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    /**
     * @brief fd 得到poller通知以后，处理事件的
     *
     * @param receiveTime
     */
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数接口
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止当channel被手动remove后，channel还在执行回调操作
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; } // 常成员函数不允许修改成员变量
    int events() const { return events_; }

    // 供Poller调用
    void set_revents(int revt) { revents_ = revt; }

    // 设置fd的相应的事件和状态 设置fd对哪些事件感兴趣
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int index) { index_ = index; }

    // one loop per thread
    // 记录当前channel属于哪个eventloop
    EventLoop *ownerLoop() { return loop_; }

    // 在channel所属的Eventloop中删除this channel
    void remove();

private:
    /**
     * @brief 当改变channel所表示fd的event事件后，update负责在poller里面更改fd相应的事件epoll_ctl
     * EventLoop ==> Channelist Poller
     */
    void update();

    /**
     * @brief 根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
     *
     * @param receiveTime
     */
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; // 事件循环
    const int fd_;    // fd，Poller监听对象 epoll_ctl
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // poller返回具体发生的事件
    int index_;       // channel的状态（是否添加到Poller） epoll_ctl是会先判断状态

    // 这里使用弱指针，是为了避免与TcpConnection循环引用：
    /**  _________________        ____________
     *  |  TcpConnection  |  +1  |   Channel  |
     *  |____unique_ptr___|<=====|_shared_ptr_|
     *           ||+1
     *       shared_ptr
     */
    std::weak_ptr<void> tie_; // 跨线程判断TcpConnetion的生存状态
    bool tied_;

    // Channel通道里面能够知道fd最终发生的具体事件的revents，所以由它负责调用具体事件的回调函数
    ReadEventCallback readCallback_; // 回调函数
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};