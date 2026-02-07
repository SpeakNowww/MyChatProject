#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Logging.h>
#include <iostream>
#include <string> 
#include <functional>
using namespace std;
using namespace muduo;  
using namespace muduo::net;

class ChatServer
{
public:
    ChatServer(EventLoop *loop, 
            const InetAddress &listenAddr,
            const string &nameArg)
            :_server(loop, listenAddr, nameArg)
            ,_loop(loop)
            {
                _server.setConnectionCallback(
                    std::bind(&ChatServer::onConnection
                    , this
                    , std::placeholders::_1)
                );
                _server.setMessageCallback(
                    std::bind(&ChatServer::onMessage
                    , this
                    , std::placeholders::_1
                    , std::placeholders::_2
                    , std::placeholders::_3)
                );
                _server.setThreadNum(2);
            }
            void start()
            {
                _server.start();
            }
private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        cout << conn->peerAddress().toIpPort() << " -> "
                << conn->localAddress().toIpPort() << " is "
                << (conn->connected() ? "Online" : "Offline") << endl;
        if(!conn->connected())
        {
            conn->shutdown();
            cout << conn->name() << conn->peerAddress().toIpPort() << " -> "
             << conn->localAddress().toIpPort() << " shutdowned!" << endl;
        }
    }

    void onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp time)
    {
        string buf = buffer->retrieveAllAsString();
        cout << "recv data: " << buf << " at " << time.toString() << endl;
        conn->send(buf);
    }
    muduo::net::TcpServer _server;
    EventLoop *_loop;
};

int main()
{
    EventLoop loop;
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatServer");
    server.start();
    loop.loop();

    return 0;
}