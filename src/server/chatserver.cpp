#include "chatserver.hpp"
#include <functional>
#include <string>
#include "json.hpp"
#include "chatservice.hpp"
#include "encrypt.hpp"
using namespace std::placeholders;
using namespace std;
using json = nlohmann::json;
ChatServer::ChatServer(EventLoop *loop,
    const InetAddress &listenAddr,
    const string &nameArg)
    : _server(loop, listenAddr, nameArg)
    , _loop(loop)
    {
        // register connection callback
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this
        , std::placeholders::_1));

        // register message callback
        _server.setMessageCallback(std::bind(&ChatServer::onMessage, this
        , std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // set the number of threads for the server
        _server.setThreadNum(4);
    }

// start the server
void ChatServer::start()
{
    _server.start();
}

// callbacks
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // client disconncted
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}


void ChatServer::onMessage(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp time)
{
    string buf = buffer->retrieveAllAsString();

    // 解密
    if(buf.size() < sizeof(MsgHeader))
    {
        conn->shutdown();
        return;
    }

    MsgHeader *header = (MsgHeader*)buf.data();

    // extract payload (after header)
    string payload = buf.substr(sizeof(MsgHeader));

    if(header->isEncrypted == 1)
    {
        string plain = aes256CBCDecrypt(payload, MY_AES_KEY);
        if(plain.empty())
        {
            conn->shutdown();
            return;
        }

        json js;
        try
        {
            js = json::parse(plain);
        }
        catch(const exception &e)
        {
            conn->shutdown();
            return;
        }

        auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
        if(msgHandler) msgHandler(conn, js, time);
        return;
    }

    // not encrypted: payload should be plain json
    try
    {
        json js = json::parse(payload);
        auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
        if(msgHandler) msgHandler(conn, js, time);
    }
    catch(const std::exception &e)
    {
        conn->shutdown();
        return;
    }
}

