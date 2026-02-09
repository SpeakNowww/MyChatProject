#include "dbtask.hpp"

DbTaskQueue& DbTaskQueue::GetInstance()
{
    static DbTaskQueue instance;
    return instance;
}

void DbTaskQueue::push(DbTaskArgs task)
{
    std::lock_guard<std::mutex> lock(dbMtx);
    dbQueue.push(std::move(task));
    _cv.notify_one();
}

DbTaskArgs DbTaskQueue::pop()
{
    std::unique_lock<std::mutex> lock(dbMtx);
    _cv.wait(lock, [this]() { return !dbQueue.empty(); });
    DbTaskArgs t = std::move(dbQueue.front());
    dbQueue.pop();
    return t;
}
    
static bool g_db_running = true;

static void dbWork()
{
    UserModel g_userModel;
    offlineMsgModel g_offlineMsgModel;
    friendModel g_friendModel;
    groupModel g_groupModel;

    while(g_db_running)
    {
        auto opt = DbTaskQueue::GetInstance().pop();
        try
        {
            switch (opt.type)
            {
                case DB_TASK_ADD_FRIEND :
                    g_friendModel.insert(opt.userid, opt.friendid);
                    break;
                case DB_TASK_ADD_GROUP_USER :
                    if (!g_groupModel.addGroup(opt.userid, opt.groupid, "normal"))
                    {
                        std::cerr << "failed to add group" << std::endl;
                    }
                    break;
                case DB_TASK_CREATE_GROUP : 
                {
                    Group group(-1, opt.name, opt.desc);
                    if (!g_groupModel.createGroup(group))
                    {
                        std::cerr << "failed to create group" << std::endl;
                    }
                    break;
                }
                case DB_TASK_INSERT_OFFLINE_MSG :
                    g_offlineMsgModel.insert(opt.toid, opt.msg);
                    break;
                case DB_TASK_UPDATE_USER_STATE : 
                {
                    User user(opt.userid, "", "", opt.state);
                    if (!g_userModel.updateState(user))
                    {
                        std::cerr << "failed to update user state" << std::endl;
                    }
                    break;
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "db task error" << std::endl;
        }
    }
}
void start_db_thread()
{
    std::thread t(dbWork);
    t.detach();
}
    