#include "poller.h"
#include "epoll_poller.h"

//单独一个.cc文件实现，因为基类包含派生类头文件是不好的实现
Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; //生成Poll实例对象
    }
    else
    {
        return new EpollPoller(loop); //生成EPoll实例对象
    }
}
