#include "eventloop.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include "poller.h"
#include "logger.h"
#include "channel.h"
#include <memory>

// 防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的polller IO的服用接口超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd，用来notify唤醒subReactor处理新来的channnel
int createEventfd()
{
    // 代替管道 一个文件描述符快速实现进程/线程通信
    // 进程通过对evtfd read/write来改变计数器的值实现进程间通信
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLopp created %p in thread %d\n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another Eventloop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd的事件回调
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个eventloop都将监听wakeupChannel的EPOLLIN读事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

/**
 * @brief 开启事件循环
 *
 */
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping\n", this);

    while (!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd 一种是client的fd 一种是wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        for (Channel *channel : activeChannels_)
        {
            // currentActiveChannel_ = channel;
            //  Poller监听哪些channel发生事件，然后上报到EventLoop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        // mainLoop accept fd（封装成channel，channel里面就已经注册好回调)=>wakeup subloop =>subloop将channel加入到channel列表中
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping\n", this);
}
/**
 * @brief 退出事件循环 1.loop在自己线程中调用quit
 *                     2.如果在其他线程中，调用quit
 *
 *                  mainLoop
 *
 *       subLoop1   subLoop2  subLoop3
 */
void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread()) // 在subloop中调用主线程quit，先将主线程从poll中唤醒，循环进行到判断quit_
    {
        wakeup();
    }
}

/**
 * @brief 在当前线程中执行cb
 *
 * @param cb 回调函数
 */
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 当前线程的loop循环中，执行cb
    {
        cb();
    }
    else // 在非当前loop执行cb，就需要唤醒loop所在线程，执行cb
    {
        queueInLoop(std::move(cb));
    }
}

/**
 * @brief 把cb放到队列中，唤醒loop所在的线程，执行cb
 *
 * @param cb 回调函数
 */
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面回调操作的loop线程
    //  callingPendingFunctors_表示当前loop正在执行回调，但是loop又有新的回调，依然需要唤醒执行新添加的回调
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // 唤醒loop所在线程
    }
}

/**
 * @brief 唤醒loop所在的线程 向wakeupfd写一个数据
 * wakeupChannel就会被唤醒
 */
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::wakeup writes %lu bytes \n", n);
    }
}

// 调用Poller方法
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

// // 判断EventLoop对象是否在自己的线程里面
// bool isInLoopThread() const { return threadId_ == CurrentThtread::tid(); }

void EventLoop::handleRead() // wake up
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead read %d bytes", n);
    }
}

void EventLoop::doPendingFunctors() // 执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_); // 减少锁带来的性能损耗
    }

    for (const Functor &functor : functors)
    {
        functor();
    }

    callingPendingFunctors_ = false;
}