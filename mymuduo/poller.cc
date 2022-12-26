#include "poller.h"
#include "channel.h"

Poller::Poller(EventLoop *loop)
    : ownerloop_(loop)
{
}

// 判断参数channnel是否在当前Poller中
bool Poller::hasChannel(Channel *channel) const
{
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}