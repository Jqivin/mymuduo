#pragma once

#include "Poller.h"

#include <vector>
#include <sys/epoll.h>

/*
* epoll的使用
* epoll_create
* epoll_ctl  add/mod/del
* epoll_wait
*/
class EpollPoller : public Poller
{
public:
    EpollPoller(EventLoop *loop);
    ~EpollPoller() override;

    //重写基类Poller的抽象方法
    TimeStamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16;   // 给EventList初始的一个长度

    //填写活跃的连接，将发生事件的channel放到activeChannels中
    void fillActiveChannels(int numEvents,ChannelList* activeChannels) const;
    // 更新channel通道，相当于调用Epoll_ctrl,第一个参数是EPOLL_CTL_ADD...
    void update(int operation,Channel* channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};