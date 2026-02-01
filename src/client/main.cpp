#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;
using namespace std::chrono;
using namespace std::this_thread;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>
#include <unordered_map>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"
#include "encrypt.hpp"
#include "chathistory.hpp"
#include "sqlite3.h"

User g_currentUser;

vector<User> g_currentUserFriendList;

vector<Group> g_currentUserGroupList;

unordered_map<uint32_t, string> g_waitAckMsgMap;

unordered_map<uint32_t, int> g_retryCountMap;

unordered_map<uint32_t, long long> g_sendTimeMap;

string g_clientId;

// const int RETRY_MAX = 3;

const int TIMEOUT_MS = 2000;

bool isMainMenuRunning = false;

// 用于读写线程之间通信
sem_t rwsem;

// 记录登陆状态
atomic_bool g_isLoginSuccess = false;

bool clientSqliteInit();

bool clientInsertChat(const string &content
    , int &sendId, int &receiveId, EnMsgType msgType);

// 消息加密
bool encryptedMessage(string &msg, EnMsgType msgType, int &clientfd, uint32_t msgId);

void checkTimeOutAndResend(int clientfd);

void onReceiveAck(uint32_t msgId);

void startTimeOutThread(int clientfd);


// 接收线程 （发送和接收在不同线程）
void readTaskHandler(int clientfd);

string getCurrentTime();

void showCurrentUserData();

// 主聊天页面程序
void mainMenu(int clientfd);

// 聊天客户端程序实现，main线程发送线程，子线程接收线程
int main()
{
    // if(argc < 3)
    // {
    //     cerr << "command invalid! command should be like ./ChatClient ip port" << endl;
    //     exit(-1);
    // }

    // 解析通过命令行参数传递的ip和port
    char fixedIp[] = "127.0.0.1";
    char *ip = fixedIp;
    uint16_t port = 8000;

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd)
    {
        cerr << "falied to create socket" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if(-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "failed to connect to server" << endl;
        close(clientfd);
        exit(-1);
    }
    cout << "successfully connected to host" << endl;

    // 初始化信号量
    sem_init(&rwsem, 0, 0);

    // 初始化本地 sqlite 数据库（聊天记录）
    if(!clientSqliteInit())
    {
        cerr << "sqlite init failed" << endl;
    }

    // 启动子线程专用接收
    std::thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    startTimeOutThread(clientfd);

    // main线程用于接收用户输入，负责发送数据
    for(;;)
    {
        cout << "=======================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "=======================" << endl;
        cout << "please choose:" << endl;
        int choice = 0;
        cin >> choice;
        cin.get(); //读掉缓冲区残留的回车


        switch (choice)
        {
            case 1: // login
            {
                int id = 0;
                char pwd[50] = {0};
                cout << "userid:";
                cin >> id;
                cin.get(); //读掉缓冲区残留的回车
                cout << "userPassword:";
                cin.getline(pwd, 50);

                json js;
                uint32_t msgId = generateUniqueMsgId(g_clientId);
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                js["msgId"] = msgId;
                string jsonStr = js.dump();
                encryptedMessage(jsonStr, LOGIN_MSG, clientfd, msgId);

                g_isLoginSuccess = false;

                // int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);

                // if(len == -1)
                // {
                //     cerr << "failed to send login message" << endl;
                // }

                sem_wait(&rwsem); // 等待信号量， 由子线程处理完登陆响应消息后，通知这里
                if (g_isLoginSuccess == true)
                {
                    isMainMenuRunning = true;
                    // 进入聊天主菜单页面
                    mainMenu(clientfd);
                }
            }
            break;
            case 2: // register
            {   char name[50] = {0};
                char pwd[50] = {0};
                cout << "username:";
                cin.getline(name, 50);
                cout << "userPassword:";
                cin.getline(pwd, 50);
                    
                json js;
                uint32_t msgId = generateUniqueMsgId(g_clientId);
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = pwd;
                js["msgId"] = msgId;
                string jsonStr = js.dump();
                encryptedMessage(jsonStr, REG_MSG, clientfd, msgId);

                // int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                // if(len == -1)
                // {
                //     cerr << "falied to send register message" << endl;
                // }
                // sem_wait(&rwsem); //等待子线程通知
            }
            break;
            case 3: // quit
                close(clientfd);
                sem_destroy(&rwsem);
                exit(0);
            default:
                cerr << "invalid input" << endl;
                break;
        }
    }
    return 0;
}

void doRegResponse(json &responsejs)
{
    if(0 != responsejs["errno"].get<int>())
    {
        cerr << "account to be registered has already existed" << endl;
    }
    else
    {
        cout << " register success, now you can log in using userid :"
        << responsejs["id"] << endl;
    }
}

void doLoginResponse(json &responsejs)
{
    if (0 != responsejs["errno"].get<int>())
    {
        cerr << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    else
    {
        g_currentUser.setId(responsejs["id"].get<int>());
        if(responsejs.contains("name") && !responsejs["name"].is_null())
            g_currentUser.setName(responsejs["name"].get<string>());
        else
            g_currentUser.setName("");

        // 记录当前用户好友列表信息
        if(responsejs.contains("friends") && !responsejs["friends"].is_null())
        {
            g_currentUserFriendList.clear();
            vector<string> vec = responsejs["friends"];
            for(string &str : vec)
            {
                json js = json::parse(str);
                User user;
                if(js.contains("id") && !js["id"].is_null()) user.setId(js["id"].get<int>());
                if(js.contains("name") && !js["name"].is_null()) user.setName(js["name"].get<string>());
                else user.setName("");
                if(js.contains("state") && !js["state"].is_null()) user.setState(js["state"].get<string>());
                else user.setState("Offline");
                g_currentUserFriendList.push_back(user);
            }
        }

        // 记录当前用户群组信息
        if (responsejs.contains("groups") && !responsejs["groups"].is_null())
        {
            g_currentUserGroupList.clear();
            vector<string> vec1 = responsejs["groups"];
            for(string &groupstr : vec1)
            {
                json groupjs = json::parse(groupstr);
                Group group;
                if(groupjs.contains("id") && !groupjs["id"].is_null()) group.setId(groupjs["id"].get<int>());
                if(groupjs.contains("groupname") && !groupjs["groupname"].is_null()) group.setName(groupjs["groupname"].get<string>());
                if(groupjs.contains("groupdesc") && !groupjs["groupdesc"].is_null()) group.setDesc(groupjs["groupdesc"].get<string>());

                if(groupjs.contains("users") && !groupjs["users"].is_null())
                {
                    vector<string> vec2 = groupjs["users"];
                    for(string &userstr : vec2)
                    {
                        groupUser user;
                        json js = json::parse(userstr);
                        if(js.contains("id") && !js["id"].is_null()) user.setId(js["id"].get<int>());
                        if(js.contains("name") && !js["name"].is_null()) user.setName(js["name"].get<string>());
                        if(js.contains("state") && !js["state"].is_null()) user.setState(js["state"].get<string>());
                        if(js.contains("role") && !js["role"].is_null()) user.setRole(js["role"].get<string>());
                        group.getUsers().push_back(user);
                    }
                }
                g_currentUserGroupList.push_back(group);
            }
        }
        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户离线消息、个人消息和群组消息
        if(responsejs.contains("offlinemsg") && !responsejs["offlinemsg"].is_null())
        {
            vector<string> vec = responsejs["offlinemsg"];
            for(string &str : vec)
            {
                json js = json::parse(str);
                int msgtype = js.contains("msgid") && !js["msgid"].is_null() ? js["msgid"].get<int>() : 0;
                if(ONE_CHAT_MSG == msgtype)
                {
                    string time = js.contains("time") && !js["time"].is_null() ? js["time"].get<string>() : "";
                    string name = js.contains("name") && !js["name"].is_null() ? js["name"].get<string>() : "";
                    string msg = js.contains("msg") && !js["msg"].is_null() ? js["msg"].get<string>() : "";
                    cout << time << " [" << js["id"] << "]" << name << ": " << msg << endl;
                }
                else if(GROUP_CHAT_MSG == msgtype)
                {
                    string time = js.contains("time") && !js["time"].is_null() ? js["time"].get<string>() : "";
                    string name = js.contains("name") && !js["name"].is_null() ? js["name"].get<string>() : "";
                    string msg = js.contains("msg") && !js["msg"].is_null() ? js["msg"].get<string>() : "";
                    cout << "群消息[" << js["groupid"] << "]: " << time << " [" << js["id"] << "]" << name << ": " << msg << endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
}

// 接收线程
void readTaskHandler(int clientfd)
{
    for(;;)
    {
        char buffer[4096] = {0};
        int len = recv(clientfd, buffer, 4096, 0);
        if(-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        string buf(buffer, len);
        if (buf.size() < sizeof(MsgHeader))
        {
            close(clientfd);
            exit(-1);
        }

        MsgHeader *header = (MsgHeader*)buf.data();
        // ensure we have the full message
        if (buf.size() < header->msgLength)
        {
            // incomplete, treat as error for now
            close(clientfd);
            exit(-1);
        }

        string payload = buf.substr(sizeof(MsgHeader), header->msgLength - sizeof(MsgHeader));
        string message;
        if (header->isEncrypted == 1)
        {
            message = aes256CBCDecrypt(payload, MY_AES_KEY);
        }
        else
        {
            message = payload;
        }

           // debug: print decrypted message (size and prefix)
           cerr << "[debug] recv msg len=" << message.size() << " content-prefix='" \
               << (message.size() > 200 ? message.substr(0,200) : message) << "'" << endl;

           // 接收chatserver转发的数据，反序列化生成json数据对象
           json js = json::parse(message);
        EnMsgType msgtype = (EnMsgType)js["msgid"].get<int>();
        string content;
        if (js.contains("msg") && !js["msg"].is_null()) content = js["msg"].get<string>();
        else content = "";


        if(msgtype == LOGIN_MSG_ACK || msgtype == REG_MSG_ACK
        || msgtype == ONE_CHAT_MSG || msgtype == GROUP_CHAT_MSG
        || msgtype == ACK_MSG)
        {
            onReceiveAck(js["msgId"].get<uint32_t>());
        }
        if(ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>() 
            << ": " << js["msg"].get<string>() << endl;
            int sendId = js["id"];
            int receiveId = js["toid"];
            clientInsertChat(content, sendId, receiveId, msgtype);
            continue;
        }
        else if(GROUP_CHAT_MSG == msgtype)
        {
            cout << "群消息[" << js["groupid"] << "]: " << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>() 
            << ": " << js["msg"].get<string>() << endl;
            int sendId = js["id"];
            int receiveId = js["groupid"];
            clientInsertChat(content, sendId, receiveId, msgtype);
            continue;
        }

        if (LOGIN_MSG_ACK == msgtype)
        {
            doLoginResponse(js); // 处理登陆响应业务，通知主线程
            sem_post(&rwsem); // 通知主线程登陆处理完成
            continue;
        }

        if (REG_MSG_ACK == msgtype)
        {
            doRegResponse(js); // 处理注册响应业务，通知主线程
            sem_post(&rwsem); // 通知主线程注册处理完成
            continue;
        }
    }
}

// 显示当前成功登陆用户基本信息
void showCurrentUserData()
{
    cout << "===================login user====================" << endl;
    cout << "current login user id: " << g_currentUser.getId() << "| name: " 
    << g_currentUser.getName() << endl;
    cout << "-------------------friend list-------------------" << endl;
    if(!g_currentUserFriendList.empty())
    {
        for(User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    } 
    cout << "-------------------group list----------------------" << endl;
    if(!g_currentUserGroupList.empty())
    {
        for(Group &group: g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " 
            << group.getDesc() << endl;
            for(groupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState() 
                << " " << user.getRole() << endl;
            }
        }
    }
    cout << "================================================" << endl;
}

string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
    (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
    (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}

void help(int fd = 0, string str = "");
// chat command handler
void chat(int, string);
// add friend command handler
void addfriend(int, string);
// creategroup command handler
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void logout(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = 
{
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat：friendid：message"},
    {"addfriend", "添加好友，格式addfriend：friendid"},
    {"creategroup", "创建群组，格式creategroup：groupname：groupdesc"},
    {"addgroup", "加入群组，格式addgroup：groupid"},
    {"groupchat", "群聊，格式groupchat：groupid：message"},
    {"logout", "注销，格式logout"}
};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = 
{
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"logout", logout}
};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();

    char buffer[4096] = {0};
    while(isMainMenuRunning)
    {
        memset(buffer, 0, sizeof(buffer));
        cin.getline(buffer, 4096);
        string commandbuf(buffer);
        string command;
        int idx = commandbuf.find(":");
        if(-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end())
        {
            cerr << "invalid input command" << endl;
            continue;
        }
        // 调用相应的命令事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1));
    }
}

// help command handler
void help(int, string)
{
    cout << "command listed below >>>" << endl;
    for(auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

// addfriend command handler
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    uint32_t msgId = generateUniqueMsgId(g_clientId);
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    js["msgId"] = msgId;
    string jsonStr = js.dump();
    encryptedMessage(jsonStr, ADD_FRIEND_MSG, clientfd, msgId);

    // int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    // if(-1 == len)
    // {
    //     cerr << "failed to send addfriend message -> " << buffer << endl;
    // }
}

void chat(int clientfd, string str)
{
    int idx = str.find(":");
    if(-1 == idx)
    {
        cerr << "chat command invalid" << endl;
        return;
    }
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1);

    json js;
    uint32_t msgId = generateUniqueMsgId(g_clientId);
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    js["msgId"] = msgId;
    string jsonStr = js.dump();
    encryptedMessage(jsonStr, ONE_CHAT_MSG, clientfd, msgId);

    // int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    // if(-1 == len)
    // {
    //     cerr << "falied to send chat message -> " << buffer << endl;
    // }
}

void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if(-1 == idx)
    {
        cerr << "creategroup command invalid" << endl;
        return;
    }

    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    uint32_t msgId = generateUniqueMsgId(g_clientId);
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    js["msgId"] = msgId;
    string jsonStr = js.dump();
    encryptedMessage(jsonStr, CREATE_GROUP_MSG, clientfd, msgId);
    // int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);

    // if(-1 == len)
    // {
    //     cerr << "failed to send creategroup message" << buffer << endl;
    // }
}

void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    uint32_t msgId = generateUniqueMsgId(g_clientId);
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    js["msgId"] = msgId;
    string jsonStr = js.dump();
    encryptedMessage(jsonStr, ADD_GROUP_MSG, clientfd, msgId);

    // int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    // if(-1 == len)
    // {
    //     cerr << "failed to send addgroup message -> " << buffer << endl;
    // }
}

void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if(-1 == idx)
    {
        cerr << "groupchat command invalid" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    uint32_t msgId = generateUniqueMsgId(g_clientId);
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    js["msgId"] = msgId;
    string jsonStr = js.dump();
    encryptedMessage(jsonStr, GROUP_CHAT_MSG, clientfd, msgId);

    // int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    // if(-1 == len)
    // {
    //     cerr << "failed to send groupchat message" << buffer << endl;
    // }
}

void logout(int clientfd, string str)
{
    json js;
    uint32_t msgId = generateUniqueMsgId(g_clientId);
    js["msgid"] = LOGOUT_MSG;
    js["id"] = g_currentUser.getId();
    js["msgId"] = msgId;
    string jsonStr = js.dump();
    if (encryptedMessage(jsonStr, LOGOUT_MSG, clientfd, msgId))
    {
        isMainMenuRunning = false;
    }

    // int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    // if(-1 == len)
    // {
    //     cerr << "failed to send logout message -> " << buffer << endl;
    // }
}

bool clientSqliteInit()
{
    // 返回 sqliteInit 的执行结果
    return sqliteInit();
}

bool clientInsertChat(const string &content
    , int &sendId, int &receiveId, EnMsgType msgType)
{
    if (insertChatHistory(content
        , sendId, receiveId, msgType))
        {
            return true;
        }
    return false;
}

// 消息加密
bool encryptedMessage(string &msg, EnMsgType msgType, int &clientfd, uint32_t msgId)
{
    string cipher = aes256CBCEncrypt(msg, MY_AES_KEY);
    if(cipher.empty()) return false;

    MsgHeader header = {0};
    header.msgLength = sizeof(MsgHeader) + cipher.size();
    header.msgType = msgType;
    header.msgId = msgId;
    header.isEncrypted = 1;

    string dataToBeSent((char*)&header, sizeof(MsgHeader));
    dataToBeSent += cipher;

    g_waitAckMsgMap[msgId] = dataToBeSent;
    g_retryCountMap[msgId] = 0;
    g_sendTimeMap[msgId] = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    int total = dataToBeSent.size(), sent = 0;
    while(sent < total)
    {
        int len = send(clientfd, dataToBeSent.c_str() + sent, total - sent, MSG_NOSIGNAL);
        if(len == -1)
        {
            cerr << "failed to send login message" << endl;
            return false;
        }
        sent += len;
    }

    return true;   
}

void checkTimeOutAndResend(int clientfd)
{
    long long now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    for(auto it = g_waitAckMsgMap.begin(); it != g_waitAckMsgMap.end(); )
    {
        uint32_t msgId = it->first;
        string &data = it->second;
        int retry = g_retryCountMap[msgId];
        long long sendTime = g_sendTimeMap[msgId];
        if(now - sendTime > TIMEOUT_MS)
        {
            if (retry >= RETRY_MAX)
            {
                cerr << "消息发送失败" << endl;
                g_retryCountMap.erase(msgId);
                g_sendTimeMap.erase(msgId);
                it = g_waitAckMsgMap.erase(it);
                continue;
            }

            int len = send(clientfd, data.c_str(), data.size(), 0);
            if (len != -1)
            {
                g_retryCountMap[msgId]++;
                g_sendTimeMap[msgId] = now;
                cout << "第" << g_retryCountMap[msgId] << "次尝试重发消息..." << endl;
            }
            else
            {
                cerr << "重发消息失败" << endl;
            }
        }
        it++;
    }
}

void onReceiveAck(uint32_t msgId)
{
    if (g_waitAckMsgMap.count(msgId))
    {
        g_waitAckMsgMap.erase(msgId);
        g_retryCountMap.erase(msgId);
        g_sendTimeMap.erase(msgId);
        cout << "发送成功" << endl;
    }
}

void startTimeOutThread(int clientfd)
{
    thread t([clientfd]()
    {
        while(true)
        {
            checkTimeOutAndResend(clientfd);
            this_thread::sleep_for(milliseconds(200));
        }
    });
    t.detach();
}
