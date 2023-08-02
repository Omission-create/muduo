#include "eventloop_threadpool.h"
#include "eventloop_thread.h"
#include <memory>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseloop, const std::string &nameArg)
    : baseloop_(baseloop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // 不需要删除loop ，栈上资源
    // Eventloop.poll()阻塞保证loop不被销毁 直至调用EventloopThread的析构函数quit_=true
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; i++)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop()); // 创建线程，绑定一个新的eventloop，并返回该loop地址
    }

    // 整个服务器只有一个线程 即baseloop
    if (numThreads_ == 0)
    {
        cb(baseloop_);
    }
}

// 如果工作在多线程中，baseloop_默认以轮询的方式分配channel给subloop
EventLoop *EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseloop_;

    if (!loops_.empty())
    {
        // round-robin
        loop = loops_[next_];
        next_ = (++next_) % loops_.size();
        // if (next_ >= loops_.size())
        // {
        //     next_ = 0;
        // }
    }

    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop *>(1, baseloop_);
    }
    else
    {
        return loops_;
    }
}
