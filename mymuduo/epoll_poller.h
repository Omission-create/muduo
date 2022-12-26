#pragma once
#include "poller.h"
#include <vector>

class Channel;

class EpollPoller : public Poller
{
public:
    EpollPoller(EventLoop *loop); //构造函数不会继承
    ~EpollPoller();

    //给所有IO服用保留统一的接口
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;

    //更新Channel
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16;

    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    //更新channel通道
    void update(int operation, Channel *channel);

    //参照epoll编程：epoll_create epoll_ctl epoll_wait
    using EventList = std::vector<struct epoll_event>;
    int epollfd_;
    EventList events_;
};