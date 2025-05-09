#pragma once

#include "noncopyable.h"
#include "TimeStamp.h"

#include <functional>
#include <memory>

class EventLoop;
class TimeStmp;
/*
 * Channel理解为通道 封装了sockfd和感兴趣的event，如EPOLLIN、EPOLLOUT
 * 还绑定了Poller返回的具体事件
 */
class Channel : noncopyable
{
public:
    // 事件回调
    using EventCallBack = std::function<void()>;
    // 只读事件回调
    using ReadEventCallBack = std::function<void(TimeStamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知以后，处理事件（调用具体的回调函数）
    void handleEvent(TimeStamp receiveTime);

    // 设置回调对象
    void setReadCallback(ReadEventCallBack cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallBack cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallBack cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallBack cb) { errorCallback_ = std::move(cb); }

    // 防止当channel被手动remove掉，channel还在执行回调操作
    void tie(const std::shared_ptr<void> &);

    int fd() const {return fd_;}
    int events() const {return events_;}
    void set_revents(int recvt) { revents_ = recvt;}

    // 设置fd感兴趣的事件
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); } 
    void enableWriteing(){ events_ |= kWriteEvent; update(); }
    void disableWriteing(){ events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ == kReadEvent; }

    int index() {return index_;}
    void set_index(int idx) {index_ = idx;}

    // one loop per thread
    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:

    void update();
    void handleEventWithGuard(TimeStamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; // 事件循环
    const int fd_;    // fd,poller监听的对象
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为channel通道里面能够获取fd最终发生的具体事件revent_,所以它负责具体的回调（用户设置的回调函数）
    ReadEventCallBack readCallback_;
    EventCallBack writeCallback_;
    EventCallBack closeCallback_;
    EventCallBack errorCallback_;
};