#include "chatservice.hpp"
#include <string>
#include <muduo/base/Logging.h>
#include <vector>
#include <map>
#include <openssl/aes.h>
#include <openssl/rand.h>
using namespace muduo;
using namespace std;

// 获取单例对象
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作    
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGOUT_MSG, std::bind(&ChatService::logout, this, _1, _2, _3)});
    // _msgHandlerMap.insert({ACK_MSG, std::bind(&ChatService::ACKToClient, this, _1, _2)});
    

    // 连接redis服务器

    // 连接redis服务器
    if (_redis.connect())
    {
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
        start_db_thread();
    }
}

// 服务器异常业务重置
void ChatService::reset()
{
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end())
    {
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid:" << msgid << "cannot find handler";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // LOG_INFO << "doing login service";
    int id = js["id"].get<int>();
    string pwd = js["password"].get<string>();
    int msgId = js["msgId"].get<int>();

    User user = _userModel.query(id);
    if(user.getId() == id && user.getPassword() == pwd)
    {
        if(user.getState() == "Online")
        {
            // 用户已登陆，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "该帐号已登陆";
            response["msgId"] = msgId;
            string jsonStr = response.dump();
            string encryptedMessage = encryptedMessage_Svr(jsonStr, GROUP_CHAT_MSG, msgId);

            conn->send(encryptedMessage);
        }
        else // 登陆成功
        {
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // 向redis订阅channel=id
            _redis.subscribe(id);
            // 设置redis在线状态
            _redis.Set(std::to_string(id), std::to_string(0), 120);

            user.setState("Online");
            DbTaskArgs t;
            t.type = DB_TASK_UPDATE_USER_STATE;
            t.userid = user.getId();
            t.state = "Online";
            DbTaskQueue::GetInstance().push(std::move(t));
            // _userModel.updateState(user);
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            response["msgId"] = msgId;

            // 查询用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 清理历史离线消息
                _offlineMsgModel.remove(id);
            }
            // 查询好友
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty())
            {
                vector<string> vec2;
                for(User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if(!groupuserVec.empty())
            {
                vector<string> groupV;
                for(Group &group : groupuserVec)
                {
                    json groupjs;
                    groupjs["id"] = group.getId();
                    groupjs["groupname"] = group.getName();
                    groupjs["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for(groupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    groupjs["users"] = userV;
                    groupV.push_back(groupjs.dump());
                }
                response["groups"] = groupV;
            }
            string jsonStr = response.dump();
            string encryptedMessage = encryptedMessage_Svr(jsonStr, GROUP_CHAT_MSG, msgId);
            conn->send(encryptedMessage);
        }
    }
    else
    {
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或密码错误";
        response["msgId"] = msgId;
        string jsonStr = response.dump();
        string encryptedMessage = encryptedMessage_Svr(jsonStr, GROUP_CHAT_MSG, msgId);
        conn->send(encryptedMessage);
    }
    ACKToClient(conn, msgId);
}
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // LOG_INFO << "doing register service";
    string name = js["name"];
    string pwd = js["password"];
    int msgId = js["msgId"].get<int>();

    User user;
    user.setName(name);
    user.setPassword(pwd);
    bool state = _userModel.insert(user);
    if(state)
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        response["msgId"] = msgId;
        string jsonStr = response.dump();
        string encryptedMessage = encryptedMessage_Svr(jsonStr, GROUP_CHAT_MSG, msgId);
        conn->send(encryptedMessage);
    }
    else
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "注册失败";
        response["msgId"] = msgId;
        string jsonStr = js.dump();
        string encryptedMessage = encryptedMessage_Svr(jsonStr, GROUP_CHAT_MSG, msgId);
        conn->send(encryptedMessage);
    }
    ACKToClient(conn, msgId);
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if(it->second == conn)
            {
                // 从map删除用户连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    
    _redis.unsubscribe(user.getId());
    _redis.Delete(std::to_string(user.getId()));

    // 更新用户状态信息
    if(user.getId() != -1)
    {
        DbTaskArgs t;
        t.type = DB_TASK_UPDATE_USER_STATE;
        t.userid = user.getId();
        t.state = "Offline";
        DbTaskQueue::GetInstance().push(std::move(t));
    }
}

void ChatService::logout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int msgId = js["msgId"].get<int>();
    
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // redis中取消订阅id
    _redis.unsubscribe(userid);
    // redis中下线用户
    _redis.Delete(std::to_string(userid));

    // User user(userid, "", "", "Offline");
    DbTaskArgs t;
    t.type = DB_TASK_UPDATE_USER_STATE;
    t.userid = userid;
    t.state = "Offline";
    DbTaskQueue::GetInstance().push(std::move(t));
    // _userModel.updateState(user);
    ACKToClient(conn, msgId);
}

void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();
    int msgId = js["msgId"].get<int>();
    string jsonStr = js.dump();
    string encryptedMessage = encryptedMessage_Svr(jsonStr, GROUP_CHAT_MSG, msgId);
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end())
        {
            // toid在线 服务器推送消息给toid用户
            it->second->send(encryptedMessage);
            // 发送方也需要收到ACK，防止客户端重试
            ACKToClient(conn, msgId);
            return;
        }
    }

    // 查询toid是否在线
    auto isOnlineOnRedis = _redis.Get(std::to_string(toid));
    if (isOnlineOnRedis)
    {
        std::cout << "successfully get from redis" << std::endl;
        _redis.publish(toid, jsonStr);
        return;
    }

    User user = _userModel.query(toid);
    if (user.getState() == "Online")
    {
        _redis.publish(toid, jsonStr);
        return;
    }

    // toid离线
    // _offlineMsgModel.insert(toid, jsonStr);
    DbTaskArgs t;
    t.type = DB_TASK_INSERT_OFFLINE_MSG;
    t.toid = user.getId();
    t.msg = jsonStr;
    DbTaskQueue::GetInstance().push(std::move(t));
    ACKToClient(conn, msgId);
}

// 添加好友
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    int msgId = js["msgId"].get<int>();
    

    DbTaskArgs t;
    t.type = DB_TASK_ADD_FRIEND;
    t.userid = userid;
    t.friendid = friendid;
    DbTaskQueue::GetInstance().push(std::move(t));
    ACKToClient(conn, msgId);
    // json response;
    // response["msgid"] = REG_MSG_ACK;
    // response["errno"] = 0;
    // response["id"] = user.getId();
    // conn->send(response.dump());
    // 其实各个方法都可以给客户端反馈一个成功/不成功信息
}

void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];
    int msgId = js["msgId"].get<int>();

    Group group(-1, name, desc);
    DbTaskArgs t;
    t.type = DB_TASK_CREATE_GROUP;
    t.name = name;
    t.desc = desc;
    DbTaskQueue::GetInstance().push(std::move(t));
    if(1)
    {
        _groupModel.addGroup(userid, group.getId(), "creator");
        json response;
        response["msgid"] = CREATE_GROUP_MSG_ACK;
        response["errno"] = 0;
        response["desc"] = "群组创建成功";
        response["msgId"] = msgId;
        string jsonStr = response.dump();
        string encryptedMessage = encryptedMessage_Svr(jsonStr, GROUP_CHAT_MSG, msgId);
        conn->send(encryptedMessage);
    }
    else
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "群组创建失败";
        response["msgId"] = msgId;
        string jsonStr = response.dump();
        string encryptedMessage = encryptedMessage_Svr(jsonStr, GROUP_CHAT_MSG, msgId);
        conn->send(encryptedMessage);
    }
    ACKToClient(conn, msgId);
}

void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    int msgId = js["msgId"].get<int>();

    DbTaskArgs t;
    t.type = DB_TASK_ADD_GROUP_USER;
    t.userid = userid;
    t.groupid = groupid;
    DbTaskQueue::GetInstance().push(std::move(t));
    if(1)
    {
        json response;
        response["msgid"] = ADD_GROUP_MSG_ACK;
        response["errno"] = 0;
        response["desc"] = "入群成功";
        response["msgId"] = msgId;
        string responseStr = response.dump();
        string encryptedMessage = encryptedMessage_Svr(responseStr, GROUP_CHAT_MSG, msgId);
        conn->send(encryptedMessage);
    }
    else
    {
        json response;
        response["msgid"] = ADD_GROUP_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "入群失败";
        response["msgId"] = msgId;
        string responseStr = response.dump();
        string encryptedMessage = encryptedMessage_Svr(responseStr, GROUP_CHAT_MSG, msgId);
        conn->send(encryptedMessage);
    }
    ACKToClient(conn, msgId);
}

void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    int msgId = js["msgId"].get<int>();
    string jsonStr = js.dump();
    string encryptedMessage = encryptedMessage_Svr(jsonStr, GROUP_CHAT_MSG, msgId);

    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    
    lock_guard<mutex> lock(_connMutex);
    for(int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end())
        {
            it->second->send(encryptedMessage);
        }
        else
        {
            // 查询toid是否在线
            User user = _userModel.query(id);
            if (user.getState() == "Online")
            {
                _redis.publish(id, jsonStr);
            }
            // else _offlineMsgModel.insert(id, jsonStr);
            else
            {
                DbTaskArgs t;
                t.type = DB_TASK_INSERT_OFFLINE_MSG;
                t.toid = id;
                t.msg = jsonStr;
                DbTaskQueue::GetInstance().push(std::move(t));
            }
        }
    }
    ACKToClient(conn, msgId);
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        OnHeartBeat(userid);
        json js = json::parse(msg);
        int msgId = js["msgId"];
        EnMsgType msgid = js["msgid"];
        string jsonStr = js.dump();
        string encryptedMessage = encryptedMessage_Svr(jsonStr, msgid, msgId);

        it->second->send(encryptedMessage);
        // ACKToClient(conn, msgId);
        return;
    }

    _offlineMsgModel.insert(userid, msg);
}

// // AES256-CBC加密 json->二进制
// string aes256CBCEncrypt_Svr(const string &plainText)
// {
//     // string cipherText;
//     AES_KEY key;
//     if (AES_set_encrypt_key(MY_AES_KEY, 256, &key) < 0) 
//     {
//         return "";
//     }

//     unsigned char iv[16];
//     RAND_bytes(iv, 16);

//     int paddingLength = 16 - (plainText.size() % 16);
//     string plainPadded = plainText;
//     plainPadded.append(paddingLength, (char)paddingLength);

//     vector<unsigned char> buf(plainPadded.size());
//     unsigned char ivCopy[16];
//     memcpy(ivCopy, iv, 16);
//     // for(size_t i = 0; i < plainPadded.size(); i+= 16)
//     // {
//     //     AES_cbc_encrypt((unsigned char*)&plainPadded[i], buf + i
//     //     , 16, &key, (unsigned char*)MY_AES_IV, AES_ENCRYPT);
//     // }
//     AES_cbc_encrypt((unsigned char*)plainPadded.data(), buf.data()
//     , plainPadded.size(), &key, ivCopy, AES_ENCRYPT);

//     string cipherText((char*)iv, 16);

//     cipherText.append((char*)buf.data(), buf.size());
//     return cipherText;
// }

// string aes256CBCDecrypt_Svr(const string &cipherText)
// {
//     if (cipherText.size() < 16)
//     {
//         return "";
//     }
//     unsigned char iv[16];
//     memcpy(iv, cipherText.data(), 16);
//     string actualCipher = cipherText.substr(16);
//     if (actualCipher.size() % 16 != 0) return "";

//     AES_KEY key;
//     if (AES_set_decrypt_key(MY_AES_KEY, 256, &key) < 0) return "";

//     vector<unsigned char> buf(actualCipher.size());
//     unsigned char ivCopy[16];
//     memcpy(ivCopy, iv, 16);
//     AES_cbc_encrypt((unsigned char*)actualCipher.data(), buf.data()
//                     , actualCipher.size(), &key, ivCopy, AES_DECRYPT);
    
//     int paddingLength = buf[actualCipher.size() - 1];
//     if (paddingLength < 1 || paddingLength > 16
//         || (size_t)paddingLength > buf.size())
//         {
//             return "";
//         }
    
//     for (size_t i = buf.size() - paddingLength; i < buf.size(); i++)
//     {
//         if (buf[i] != paddingLength) return "";
//     }
//     return string((char*)buf.data(), buf.size() - paddingLength);
// }

string encryptedMessage_Svr(string &msg, EnMsgType msgType, uint32_t msgId)
{
    string cipher = aes256CBCEncrypt(msg, MY_AES_KEY);
    if (cipher.empty()) return "";
    MsgHeader header = {0};
    header.msgLength = sizeof(MsgHeader) + cipher.size();
    header.msgType = msgType;
    header.msgId = msgId;
    header.isEncrypted = 1;

    string dataToBeSent((char*)&header, sizeof(MsgHeader));
    dataToBeSent += cipher;
    return dataToBeSent;
}


// 回传客户端ACK
void ChatService::ACKToClient(const TcpConnectionPtr &conn, int &msgId)
{
    json response;
    response["msgId"] = msgId;
    response["msgid"] = ACK_MSG;
    string jsonStr = response.dump();
    string encryptedMessage = encryptedMessage_Svr(jsonStr, GROUP_CHAT_MSG, msgId);
    conn->send(encryptedMessage);
}

// redis心跳续期
void ChatService::OnHeartBeat(int id)
{
    _redis.Set(std::to_string(id), std::to_string(0), 120);
}