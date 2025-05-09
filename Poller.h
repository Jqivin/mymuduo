#pragma once

#include "noncopyable.h"
#include "TimeStamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模板
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop*);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一的接口
    virtual TimeStamp poll(int timeoutMs,ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;
    
    // 判断参数channel是否在当前Poller中
    bool hasChannel(Channel* channel) const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现
    // 注意这个：肯定是要返回具体类型的，比如（Epoller.h）,为了不在基类中引用派生类的头文件，所以此函数放到派生类中实现；
    static Poller* newDefaultPoller(EventLoop* loop);


protected:
    // map的key：sockfd  value：sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_;      // 定义Poller所属的事件循环EventLoop
};