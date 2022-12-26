#pragma once

#include "noncopyable.h"
#include <functional>
#include "thread.h"
#include <mutex>
#include <condition_variable>
#include <string>

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = "");
    ~EventLoopThread();

    /**
     * @brief 创建新的loop和线程
     *
     * @return EventLoop* 返回新创建的loop对象指针
     */
    EventLoop *startLoop();

private:
    void threadFunc();

    EventLoop *loop_;
    bool exititng_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};