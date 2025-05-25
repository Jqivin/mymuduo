#include "Acceptor.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <error.h>

static int createNonblockingOrDie()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d create socket failed:%d \n",__FILE__,__FUNCTION__,__LINE__,errno );
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr,bool reuseport)
    :loop_(loop)
    , acceptSocket_(createNonblockingOrDie())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    // TcpServer::start() -> Acceptor::listen() 有新用户的连接，需要执行一个回调，把conFd -> 打包channel -> 唤醒subloop
    // baseloop ->acceptChannel(listenfd) ->ReadCallback -> Acceptor::handleRead
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));

}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    // 监听acceptChannel的读事件
    acceptChannel_.enableReading();
    LOG_INFO("Acceptor::listenfd listenning %d \n",acceptSocket_.fd());
}

/*
执行时机：listenfd有事件发生，也就是有新用户的连接了
*/
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd,peerAddr);
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("accept error:%d \n",errno);
        if(errno == EMFILE)
        {
            LOG_ERROR("socket is full");
        }
    }
}