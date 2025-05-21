#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{

}
EventLoopThreadPool:: ~EventLoopThreadPool()
{

}

void  EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    started_ = true;
    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size() + 32];
        // 线程池名字+下标
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.emplace_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(threads_[i]->startLoop()); // 底层创建线程，绑定一个新的EventLoop，返回该loop地址
    }

    // 没有设置线程数量，直接使用baseLoop_，也就是主线程
    if(numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}

//如果工作在多线程中，baseLoop_会默认以轮询的方式分配channel给subloop
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop = baseLoop_;
    if(!loops_.empty())
    {
        loop = loops_[next_];  // 轮询获取下一个EventLoop对象
        ++next_;  // 下标加1
        if(next_ >= static_cast<int>(loops_.size()))  // 如果下标超过了线程池的大小，重新开始
        {
            next_ = 0;
        }
    }
    return loop;
}
std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if(loops_.empty())
    {
       return std::vector<EventLoop*>(1, baseLoop_); // 如果没有线程池，返回主线程的EventLoop对象
    }
    else
    {
        return loops_;
    }
}
