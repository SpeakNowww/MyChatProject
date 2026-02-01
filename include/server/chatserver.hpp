#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
using namespace muduo;
using namespace muduo::net;
using namespace std;

class ChatServer
{
public:
// initialize chatserver object
    ChatServer(EventLoop *loop, 
        const InetAddress &listenaddr, 
        const string &nameArg);

        void start();
private:
    // callbacks
    void onConnection(const TcpConnectionPtr&);
    void onMessage(const TcpConnectionPtr&, Buffer*, Timestamp);

    TcpServer _server; // 实现服务器功能的类对象
    EventLoop *_loop; // 指向事件循环对象的指针
};

// constexpr unsigned char MY_AES_KEY[] = "1234567890ABCDEF1234567890ABCDEF";
// constexpr unsigned char MY_AES_IV[] = "1234567890ABCDEF";

// string aes256CBCDecrypt_Svr(const string &cipherText);
#endif