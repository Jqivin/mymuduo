#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <stdlib.h>
#include <error.h>
#include <fcntl.h>
#include <memory>

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

    while(!quit_)
    {
        // 每次epoll发生事件时候会返回发生事件的channel
        activeChannels_.clear();
        /*
        * 监听两类fd， 
            clientfd -- 与客户端通信的channel
            wakeupfd  -- mainloop唤醒subloop的channel
        */
        pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_);
        
        // 遍历活跃的channel
        for(Channel* channel : activeChannels_)
        {
            // poller监听那些channel发生事件了，上报给EventLoop，通知相应的channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前Eventloop事件循环需要处理的回调操作
        /*
        * IO线程（mainReactor，主要做接收新用户的连接accept，我们肯定使用一个channel打包返回的fd，已连接的channel要分发给subReactor，服务器肯定是用多线程的）
        * mainLoop事先注册一个callback，这个cb需要subloop执行，但是现在subloop还在睡眠，所以需要mainloop把subloop给wakeup
        *  subloop起来之后在这个函数做的事情就是doPendingFunctors  --  执行之前mainLoop给注册的回调，回调都在pendingFunctors_里写的
        */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping!\n",this);
    looping_ = false;
}

// 退出事件循环
void EventLoop::quit()
{
    quit_ = true;

    /*
        1.loop在自己的线程中调用quit，这时候可定没有阻塞到poll函数中，这时候可以推出poll函数，也就是退出当前线程
        2.在其他线程中调用的quit（可能subloop调用mainloop的quit），这时候当前loop可能阻塞到poll函数的epoll_wait
        也就是，在非loop的线程中调用loop的quit，所以需要将其唤醒，这时候loop线程走到poll函数中才能退出（quit_在上面已经设置为true）
        每一个loop中都有一个wakeupfd，可以用于唤醒
    */
    if(!isInLoopThread())
    {
        wakeup();
    }
}

// 在当前loop中执行
void EventLoop::runInLoop(Functor cb)
{
    if(isInLoopThread()) // 在当前的loop线程中执行cb
    {
        cb();
    }
    else
    {// 在非当前loop线程中执行cb，就需要唤醒loop所在线程执行cb
        queueInLoop(cb);
    }
}
// 把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的需要执行上面回调操作的loop的线程
    // || callingPendingFunctors_：当前loop正在执行回调，又给当前looptian'ji
    if(!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();  // 唤醒loop所在的线程
    }
}

// mainReactor唤醒subReactor，也就是唤醒loop所在的线程

/*
向wakeupfd写一个数据，当前loop已经注册过wakeupfd的读事件，写数据后，就会走到wakeup的回调函数
wakeupChannel就发生读事件，当前loop线程就会被唤醒
*/
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_,&one,sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("Eventloop::wakeup writes %lu bytes instead of 8 \n",n);
    }
}

// EventLoop的方法 - 》 poller的方法
void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> locker(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor& functor : functors)
    {
        functor();   // 执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}