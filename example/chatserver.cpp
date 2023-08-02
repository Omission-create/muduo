/*

muduo网络库主要给用户提供两个类
TcpServer:用于编写服务器程序
TcpClient:用于编写客户端程序

epoll + 线程池
将网络I/O和业务代码（用户的连接和断开 用户的可读写事件）区分开

*/
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <functional>
#include <iostream>
#include <string>
using namespace std;
using namespace muduo;
using namespace net;
using namespace placeholders;

// 基于muduo网络库开发服务器程序
/*
1 组合TcpServer对象
2 创建EventLoo事件循环对象的指针
3 明确TcpServer构造函数需要什么参数，输出ChatServer的构造函数
4 在当前服务其中注册处理连接的函数和处理读写的函数
5 设置合适的服务端线程数量，muduo库会自己分配I/O线程和worker线程的数量
*/

class ChatServer
{
private:
    TcpServer server_; // 1 组合TcpServer对象
    EventLoop *loop_;  // 2 创建EventLoo事件循环对象的指针

    // 专门处理用户读写事件
    void onMessage(const TcpConnectionPtr &conn, // 连接
                   Buffer *buf,                  // 缓冲区
                   Timestamp time)               // 接收到数据的事件信息
    {
        string s = buf->retrieveAllAsString();
        cout << "recv data: " << s << "time: " << time.toString() << endl;
        conn->send(s);
    }

    // 专门处理用户的连接和断开 epoll listenfd
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            cout << conn->peerAddress().toIpPort() << "->" << conn->localAddress().toIpPort() << "state : online" << endl;
        }
        else
        {
            cout << conn->peerAddress().toIpPort() << "->" << conn->localAddress().toIpPort() << "state : offline" << endl;
            conn->shutdown(); // close(fd)
            // loop_->quit();
        }
    }

public:
    ChatServer(EventLoop *loop,                           // 事件循环
               const InetAddress &listenAddr,             // IP + port
               const string &nameArg)                     // 服务器名字
        : server_(loop, listenAddr, nameArg), loop_(loop) // TcpServer没有默认构造函数
    {
        // 给服务器注册用户连接的创建和断开回调
        server_.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1)); // 4 在当前服务其中注册处理连接的函数和处理读写的函数

        // 给服务器注册用户读写回调事件回调
        // 动态成员函数bind this指针
        server_.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

        // 设置服务器的线程数量 1个I/O线程 3个worker线程
        server_.setThreadNum(4); // 5 设置合适的服务端线程数量，muduo库会自己分配I/O线程和worker线程的数量
    }
    ~ChatServer();

    // 开启事件循环
    void Start()
    {
        server_.start();
    }
};

ChatServer::~ChatServer()
{
}

int main()
{
    EventLoop loop; // baseloop
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatSercer");

    server.Start(); // listenfd epoll_ctl=>epoll
    loop.loop();    // epoll_wait以阻塞的方式等待新用户连接，已连接的用户的读写事件等
    return 0;
}