#pragma once

#include "noncopyable.h"

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "TimeStamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;
/*
* 事件循环类  
* 主要包含两大模块：Channel(连接通道)  Poller(Epoll、poll的抽象)
*/
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    TimeStamp pollReturnTime() const {return pollReturnTime_;}

    // 在当前loop中执行
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);

    // mainReactor唤醒subReactor，也就是唤醒loop所在的线程
    void wakeup();

    // EventLoop的方法 - 》 poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 主要用于判断该EventLoop对象是否在自己的线程里面，在当前线程的时候才去执行相应操作
    bool isInLoopThread() const {return threadId_ == CurrentThread::tid();}
private:
    using ChannelList = std::vector<Channel*>;

    // wakeup
    void handleRead();

    // 主要是打印下活跃的channel
    void doPendingFunctors();

    std::atomic_bool looping_;           // 原子操作 通过CAS实现
    std::atomic_bool quit_; // 标识推出loop循环
    
    const pid_t threadId_;      //记录当前loop所在的线程的ID ，每一个eventloop都是一个线程
    TimeStamp pollReturnTime_;  //poller返回事件发生channels的时间
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;      // 当mainLoop获取一个新用户的channel。通过轮询算法选择一个subloop，通过该成员唤醒subloop处理channel(每个subReactor都监听wakefd)。通过系统调用eventfd，线程间的通信机制，效率比较高

    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;  //eventLoop管理的所有的channel

    std::atomic_bool callingPendingFunctors_;   // 标识当前loop是否需要执行的回调操作
    std::vector<Functor> pendingFunctors_;   // 存储Loop需要执行的所有的回调操作
    std::mutex mutex_;      // 回调操作，用来保护上面vector容器的线程安全的
};