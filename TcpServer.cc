#include "TcpServer.h"
#include "Logger.h"

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
        Option option = kNoReusePort)
        : loop_(loop)
        , name_(nameArg)
        , ipPort_(listenAddr.toIpPort())
        , acceptor_(new Acceptor(loop,listenAddr,option))
        , threadPool_(new EventLoopThreadPool(loop,name_))
        , connectionCallback_()
        , messageCallback_()
        , nextConnId_(1)
        , started_(0)
{
    // 设置newConnecttion回调，当有新用户连接的时候，会执行这个回调
}

TcpServer::~TcpServer()
{

}

void TcpServer::setThreadNums(int threadNums)
{
    
}