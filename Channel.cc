#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// EventLoop（一个事件循环，一个线程中） 包含多个Channel和一个Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

Channel::~Channel(){}

// 调用时机 -- 待说明
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied_ = true;
}

/*
* 当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件（epoll_ctl）
* 通过EventLoop更新  EventLoop中有 channellist 和 poller
*/
void Channel::update()
{
    // 通过channel所属的EventLoop,调用poller的相应方法，注册fd的events事件
    // add code todo
    //loop_->updatechannel(this);
}

/*
* 在channel所属的Eventloop中删除当前的channel
*/
void Channel::remove()
{
    //loop_->removechannel(this);
}

// fd得到poller的通知以后，处理相应的事件
void Channel::handleEvent(TimeStamp receiveTime)
{
    if(tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
        else
        {
            handleEventWithGuard(receiveTime);
        }
    }
}

// 根据poller通知的channel发生的具体事件，由channel负责调用具体的回调
void Channel::handleEventWithGuard(TimeStamp receiveTime)
{
    LOG_INFO("channel handle event:%d",revents_);

    // EPOLLHUP 表示连接挂起，通常意味着连接已关闭
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
    }

    if(revents_ & EPOLLERR)
    {
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}
