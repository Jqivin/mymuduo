#include "EventLoopThread.h"
#include "EventLoop.h"

#include <memory>

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb , const std::string& name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = false;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }

}

EventLoop* EventLoopThread::startLoop()
{
    // 启动底层新线程
    thread_.start();

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);  // 上锁
        while (loop_ == nullptr)  // 等待EventLoop对象创建完成
        {
            cond_.wait(lock);  // 等待条件变量,也就是等新的线程创建完毕，才能使用loop_
        }
        loop = loop_;
    }

    return loop;  // 返回EventLoop对象
    // 这里的loop_是一个独立的EventLoop对象，和上面的线程是一一对应的
}

// 在单独的新线程里面运行的
void EventLoopThread::threadFunc()
{
    EventLoop loop;  // 创建一个独立的EventLoop，和上面的线程是一一对应的，one loop per thread
    if(callback_)
    {
        callback_(&loop);  // 传入EventLoop对象
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;  // 让EventLoopThread对象拥有EventLoop对象的指针
        cond_.notify_one();  // 唤醒
    }
    loop.loop();  // 开启事件循环,while循环

    {
        // loop已经退出了，服务器程序要关闭了，不进行事件循环了
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = nullptr;
    }
}