#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <stdlib.h>
#include <error.h>
#include <fcntl.h>

 // __thread 保证全局变量t_cachedTid在不同的线程中有不同的值，C++11 中可以使用thread_local 关键字
 // 此变量用于确保一个线程中只能创建一个EventLoop，当为空的时候才会创建
__thread EventLoop* t_loopInThisThread = nullptr;

// 定义默认的Poller的超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int createEventFd()
{
    int eventfd = ::eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
    if(eventfd < 0)
    {
        LOG_FATAL("create fd error:%d \n",errno);
    }

    return eventfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventFd())
    , wakeupChannel_(new Channel(this,wakeupFd_))
    , currentActiveChannels_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d",this,threadId_);
    if(t_loopInThisThread)
    {
        // 当前线程已经有一个loop了
        LOG_FATAL("Another Eventloop %p exists in this thread %d \n",t_loopInThisThread,threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 注册wakeupfd的回调函数和设置它的感兴趣的事件
    // wakeupfd主要的作用就是唤醒subReact（Eventloop）
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    // 每一个Eventloop都将监听wakeupfd的EPOLLIN读事件，也就是mainReactor可以通过write通知subReactor
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    // 对所有的事件都不感兴趣
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_,&one,sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead reads %d bytes instead of 8",n);
    }
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping",this);
}

// 退出事件循环
void EventLoop::quit()
{

}

 