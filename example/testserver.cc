#include "../TcpServer.h"
#include "../Logger.h"

#include <string>
#include <functional>

/**
 * @brief 基于精简的muduo网络库创建EchoServer
 *
 * EchoServer的特点：客户端发什么，服务端就回复什么
 *
 */
class EchoServer
{
public:
    EchoServer(EventLoop *loop,         // 传入 Base-EventLoop ，对应Acceptor模块
               const InetAddress &addr, // 传入服务器监听的IP地址和端口号
               const std::string &name) // 给当前 EchoServer 起一个别名
        : server_(loop, addr, name),    // 初始化 TcpServer 成员变量
          loop_(loop)                   // loop_是EventLoop*指针，用于绑定BaseLoop
    {
        // 设置server_的回调函数 - 连接建立/断开时执行的内容
        server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));

        // 设置server_的回调函数 - 收到客户端发来的消息后执行的内容
        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this,
                                             std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的loop线程数量 loopthread
        server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }

private:
    // 回调函数 - 连接建立/断开时执行的内容
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 回调函数 - 收到客户端发来的消息后执行的内容
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown(); // 写端   EPOLLHUP =》 closeCallback_
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;                                  // 定义Base-EventLoop，对应Acceptor模块
    InetAddress addr(8000);                          // 初始化服务器监听的IP地址和端口号
    EchoServer server(&loop, addr, "EchoServer-01"); // Acceptor non-blocking listenfd  create bind
    server.start();                                  // listen  loopthread  listenfd => acceptChannel => mainLoop =>
    loop.loop();                                     // 启动mainLoop的底层Poller

    return 0;
}