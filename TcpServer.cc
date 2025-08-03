#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <strings.h>
#include <functional>

static EventLoop* CheackLoopNotNull(EventLoop* loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainloop is nullptr \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop,
        const InetAddress &listenAddr,
        const std::string &nameArg,
        Option option)
        : loop_(CheackLoopNotNull(loop))
        , name_(nameArg)
        , ipPort_(listenAddr.toIpPort())
        , acceptor_(new Acceptor(loop,listenAddr,option == kReusePort))
        , threadPool_(new EventLoopThreadPool(loop,name_))
        , connectionCallback_()
        , messageCallback_()
        , nextConnId_(1)
        , started_(0)
{
    // 设置newConnecttion回调，当有新用户连接的时候，会执行这个回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,this
                                        ,std::placeholders::_1
                                        ,std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for(auto &item : connections_)
    {
        // 这个局部的shared_ptr智能指针对象，出右括号，可以自动释放new出来的TcpConnection对象资源了
        TcpConnectionPtr conn(item.second); 
        item.second.reset();

        // 销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed,conn)
        );
    }
}

void TcpServer::start()
{
    if(started_++ == 0)
    {
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get()));
    }
}

void TcpServer::setThreadNums(int threadNums)
{
    threadPool_->setThreadNum(threadNums);
}

// 有一个新的客户端的连接，acceptor会执行这个回调操作 在TcpServer的构造函数中设置了这个回调函数
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    //  轮询算法，选择一个subLoop，来管理channel
    EventLoop* ioloop = threadPool_->getNextLoop();

    char buf[64] = {0};
    snprintf(buf,sizeof buf,"-%s#%d",ipPort_.c_str(),nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

        // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local,sizeof local);
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd,(sockaddr*)&local,&addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    // 根据连接成功的sockfd，创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(ioloop,
                                connName,
                                sockfd,
                                localAddr,
                                peerAddr));
    connections_[connName] = conn;

    // 给TcpConnection对象设置回调，这些都是用户设置给TcpServer的
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭的回调 当coon->shutdown的时候调用
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection,this,std::placeholders::_1));

    // 直接调用TcpConnection::connectEstablished
    ioloop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn)
    );
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", 
        name_.c_str(), conn->name().c_str());
    
    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed,conn)
    );
}