#pragma once
#include "noncopyable.h"
#include <functional>
#include <vector>
#include <atomic>
#include <unistd.h>
#include "timestamp.h"
#include <memory>
#include <mutex>
#include "current_thread.h"

class Poller;
class Channel;

// per thread per loop
// 事件循环类 主要包含两个模块 Channel Pooller（封装epoll）
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    /**
     * @brief 开启事件循环
     *
     */
    void loop();
    /**
     * @brief 退出事件循环
     *
     */
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    /**
     * @brief 再当前线程中执行cb
     *
     * @param cb 回调函数
     */
    void runInLoop(Functor cb);
    /**
     * @brief 把cb放到队列中，唤醒loop所在的线程，执行cb
     *
     * @param cb 回调函数
     */
    void queueInLoop(Functor cb);

    /**
     * @brief 唤醒loop所在的线程
     *
     */
    void wakeup();

    // 调用Poller方法
    void removeChannel(Channel *channel);
    void updateChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead();        // wake up
    void doPendingFunctors(); // 执行回调

    using ChannelList = std::vector<Channel *>;
    std::atomic_bool looping_; // 原子操作 通过CAS实现
    std::atomic_bool quit_;    // 标志退出loop循环
    const pid_t threadId_;     // 记录当前loop所在线程的id
    Timestamp pollReturnTime_; // poller返回发生时间的channels的时间点

    // one loop one poller
    std::unique_ptr<Poller> poller_;

    int wakeupFd_; // 当mainLoop获取一个新用户的channel，通过轮询选择一个subLoop处理线程
    std::unique_ptr<Channel> wakeupChannel_;

    // poll返回的有revents的channel
    ChannelList activeChannels_;
    // Channel *currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_; // 表示当前loop是否有需要执行回调的地方
    std::vector<Functor> pendingFunctors_;    // 存储loop需要执行的所有回调操作
    std::mutex mutex_;                        // 线程安全
};