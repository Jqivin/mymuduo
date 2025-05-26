#pragma once

#include "noncopyable.h"

#include <functional>
#include <vector>
#include <memory>
#include <string>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
    using ThreadInitCallback = std::function<void(EventLoop*)>;
public:
    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }  // 设置线程数量
    void start(const ThreadInitCallback& cb = ThreadInitCallback());  // 启动线程池

    //如果工作在多线程中，baseLoop_会默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop();  // 获取下一个EventLoop对象
    std::vector<EventLoop*> getAllLoops();  // 获取所有的EventLoop对象
    bool started() const { return started_; }  // 是否已经开始
    const std::string& name() const { return name_; }  // 获取线程池的名字

private:
    EventLoop* baseLoop_;  // 用户刚开始创建的EventLoop对象 ，设置线程数量，也就能利用多核资源，就会通过这个EventLoop创建其他的EventLoop
    std::string name_;  // 线程池的名字
    bool started_;  // 是否已经开始
    int numThreads_;  // 线程池的线程数量
    int next_;  // 下一个线程的索引,轮询下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_;  // 线程池
    std::vector<EventLoop*> loops_;  // 线程池里面的EventLoop对象，不需要delete，因为是栈上管理的，在EventLoopThrad中创建的
};