#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

// channel未添加到poller中
const int kNew = -1;   // channel的成员index初始化也是-1
// channel已添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDelete = 2;

// 显式使用 :: 是一种良好的编程习惯，它能清晰表明这是一个系统调用或全局函数，而非类的成员函数或局部函数。这在阅读代码时能提高可读性，减少歧义
EpollPoller::EpollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create1 error:%d \n", errno);
    }
}

EpollPoller::~EpollPoller()
{
    ::close(epollfd_);
}

TimeStamp EpollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    LOG_INFO("fd total count:%d \n",(int)channels_.size());

    int numEvents = ::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
    int saveErrno = errno;

    TimeStamp now(TimeStamp::now());

    if(numEvents > 0)
    {// 有已经发生相应事件的fd
        LOG_INFO("%d events happened \n",numEvents);
        fillActiveChannels(numEvents,activeChannels);
        if(numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if(numEvents == 0)
    {// 没有发生相应事件
        LOG_DEBUG("nothing happend ! \n");
    }
    else
    {
        // error happened
        if(saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EpollPoller::poll err!\n");
        }
    }
    return now;
}

// Channel的update和remove通过调用EventLoop的update和remove，来调用此函数
/*
* Eventloop里  ChannelList（所有的channel）   Poller(ChannelMap<fd,channel*>)
 ChannelList的个数 >= Poller中的ChannelMap, = 的情况就是所有的channel都注册到了Poller上
*/
void EpollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("fd = %d event=%d index=%d \n",channel->fd(),channel->events(),index);
    if(index == kNew || index == kDelete )
    {// fd从未添加到Poller中或者已经从Poller中删除
        if(index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD,channel);
    }
    else
    {// channel已经添加到Epoller中了
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            // 对任何事件都不感兴趣了
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(kDelete);
        }
        else
        {
            update(EPOLL_CTL_MOD,channel);
        }
    }
} 

void EpollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    // 从map中删除
    channels_.erase(fd);

    int index = channel->index();
    if(index == kAdded)
    {
        // 从epoll中删除
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EpollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for(int i = 0; i < numEvents; ++i)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        if(channel != nullptr)
        {
            channel->set_revents(events_[i].events);
            activeChannels->push_back(channel);  // EventLoop 就拿到了epoller给他返回的所有发生事件的channel列表了
        }
    }
}

// 更新channel通道，相当于调用Epoll_ctrl , 
void EpollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    memset(&event,0,sizeof event);
    event.events = channel->events();
    int fd = channel->fd();
    event.data.fd = fd;
    event.data.ptr = channel;

   
    if(::epoll_ctl(epollfd_,operation,fd,&event) < 0)
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl delete error:%d\n",errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n",errno);
        }
    }
}