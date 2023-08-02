#include "epoll_poller.h"
#include "channel.h"
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include "logger.h"
#include <unistd.h>

// channel未添加到poller中
const int kNew = -1; // channel的成员index_=-1
// channel已添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

// Poller只有带参构造函数，子类必须显示调用父类带参构造函数
EpollPoller::EpollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("EpollPoller::EpollPoller epoll_create failed errno:%d\n", errno);
    }
}

EpollPoller::~EpollPoller()
{
    ::close(epollfd_);
}

/**
 * @brief 监听事件
 *
 * @param timeoutMs 超时时间ms
 * @param activeChannels 将监听到的事件列表填到ChannelList，告知EventLoop
 * @return Timestamp
 */
Timestamp EpollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    LOG_DEBUG("EpollPoller::poll  channels_.size %d\n", channels_.size());

    int numEvents = ::epoll_wait(epollfd_,
                                 &*events_.begin(), // 使用vector兼容数组
                                 static_cast<int>(events_.size()),
                                 timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());
    if (numEvents > 0)
    {
        LOG_DEBUG("%s numEvents %d\n", __FUNCTION__, numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()) // 发生时间数可能大于vector.size()，扩容
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    }
    else
    {
        if (savedErrno != EINTR)
        {
            errno = savedErrno;
            LOG_ERROR("%s failed!\n", __FUNCTION__);
        }
    }
    return now;
}

// 更新Channel channel update remove => Eventloop =>Poller
void EpollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index(); // channel的状态（是否添加到Poller）
    LOG_INFO("%s => fd=%d events=%d index=%d\n", __FUNCTION__, channel->fd(), channel->events(), index);

    // 新的channel先放到ChannelMap中，再注册到Poller
    // 已经从Poller中删除的channel，虽然还在Map中，但需要重新注册到Poller
    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent()) // channel已经在Map里面，但是对任何事件都不感兴趣
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从ChannelMap里面删除，不是从Poller里面delete
void EpollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    int index = channel->index();
    channels_.erase(fd);
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EpollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; i++)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EveneLoop就拿到了它的poller给它返回的所有发生事件的channel列表
    }
}

// 更新channel通道
void EpollPoller::update(int operation, Channel *channel)
{
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
            LOG_ERROR("func=%s => epoll_ctl delete errono:%d\n", __FUNCTION__, errno);
        else
            LOG_FATAL("func=%s => epoll_ctl add/modify errono:%d\n", __FUNCTION__, errno);
    }
}