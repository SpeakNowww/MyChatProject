#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <string>
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include <thread>
#include <iostream>

enum DbTaskType
{
    DB_TASK_UPDATE_USER_STATE,
    DB_TASK_INSERT_OFFLINE_MSG,
    DB_TASK_ADD_FRIEND,
    DB_TASK_CREATE_GROUP,
    DB_TASK_ADD_GROUP_USER
};

struct DbTaskArgs
{
    DbTaskType type;
    int userid = -1;
    int friendid = -1;
    std::string state;
    std::string msg;
    int toid = -1;
    int groupid = -1;
    std::string name, desc, role;
};

class DbTaskQueue
{
public:
    static DbTaskQueue& GetInstance();
    void push(DbTaskArgs task);
    DbTaskArgs pop();
private:
    DbTaskQueue() = default;
    std::queue<DbTaskArgs> dbQueue;
    std::mutex dbMtx;
    std::condition_variable _cv;
};

void start_db_thread();


