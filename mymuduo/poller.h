#pragma once

#include <vector>
#include <unordered_map>
#include "timestamp.h"
#include "noncopyable.h"

class Channel;
class EventLoop;

/**
 * @brief muduo库中多路事件分发器的核心IO复用模块
 *
 */
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeouts, ChannelList *activeChannels) = 0;

    // 更新Channel
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数channnel是否在当前Poller中
    bool hasChannel(Channel *channel) const;

    // EvenntLoop可以通过该接口获取默认的IO复用的具体实现
    // one loop one poller 不需要线程安全
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    // sockfd : Channel
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;

private:
    EventLoop *ownerloop_; // 定义Poller所属的事件循环EventLoop
};
