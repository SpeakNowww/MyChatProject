#ifndef CHATHISTORY_H
#define CHTAHISTORY_H
#include "user.hpp"
#include "public.hpp"
#include "sqlite3.h"
#include <iostream>

using namespace std;

// sqlite初始化
bool sqliteInit();

// 聊天记录插入
bool insertChatHistory(const string &content
                        , int &sendId, int &receiveId, EnMsgType msgType);

// 消息加密传输
void msgencrypt(const string &msg);

#endif