#pragma once
#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class Channel;
class InetAddress;

/*
acceptor类的作用是监听新连接,用的EventLoop就是用户定义的那个baseLoop，也就是mainReactor
*/
class Acceptor : noncopyable
{
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
public:
    Acceptor(EventLoop* loop, const InetAddress& listenAddr,bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    {
        newConnectionCallback_ = cb;
    }

    bool listening() const { return listenning_; }
    void listen();
private:
    void handleRead();
    
    int listenfd_;
    bool listenning_;
   
    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
};