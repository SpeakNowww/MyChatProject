#ifndef CHATSERVICE_H
#define CHATSERVICE_H
#include <unordered_map>
#include <functional>
#include <mutex>
#include <muduo/net/TcpConnection.h>
#include "json.hpp"
#include "redis.hpp"
#include "usermodel.hpp"
#include "groupmodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "public.hpp"
#include "encrypt.hpp"
#include "dbtask.hpp"
using namespace muduo;
using namespace muduo::net;
using namespace std;
using json = nlohmann::json;
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp time)>;

// 聊天服务器业务类
class ChatService
{
public:
    // 单例对象接口函数
    static ChatService* instance();
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);

    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);

    // 添加好友业务
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);

    // 创建群组
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);

    // 加入群组
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);

    // 群组聊天
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);

    // 处理注销业务
    void logout(const TcpConnectionPtr &conn, json &js, Timestamp time);

    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);

    // 服务器异常业务重置
    void reset();

    // 从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int userid, string msg);
    
    // 回传客户端ACK
    void ACKToClient(const TcpConnectionPtr &conn, int &msgId);

    // redis心跳续期
    void OnHeartBeat(int id);
private:
    ChatService();
    // 消息id-》业务处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap;

    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;

    // 互斥锁
    std::mutex _connMutex;

    // 数据操作类对象
    UserModel _userModel;
    offlineMsgModel _offlineMsgModel;
    friendModel _friendModel;
    groupModel _groupModel;

    // redis对象
    Redis _redis;
};

// string aes256CBCEncrypt_Svr(const string &plainText);
// string aes256CBCDecrypt_Svr(const string &cipherText);
string encryptedMessage_Svr(string &msg, EnMsgType msgType, uint32_t msgId);
#endif