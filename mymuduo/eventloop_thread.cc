#include "eventloop_thread.h"
#include "eventloop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr),
      exititng_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb)
{
}
EventLoopThread::~EventLoopThread()
{
    exititng_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop()
{
    thread_.start(); // 启动线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr)
        {
            cond_.wait(lock); // wait原子操作：释放锁;等待条件变量
        }
        loop = loop_;
    }
    return loop;
}

// thread_->start => 【子线程中调用】thread_->func() => EventLoopThread::threadFunc()
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个独立的eventloop，one loop one thread

    // 初始化回调初始化loop
    if (callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); // EventLoop.loop => Poller.poll
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}