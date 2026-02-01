#include "chathistory.hpp"

using namespace std;

// sqlite初始化
bool sqliteInit()
{
    sqlite3 *db;
    int rc = sqlite3_open("./client_chat.db", &db);
    if (rc != SQLITE_OK)
    {
        cerr << "cannot open database: " << sqlite3_errmsg(db) << endl;
        if (db) sqlite3_close(db);
        return false;
    }

    // create table: id auto-increment primary key, and chat fields
    const char *sql = "CREATE TABLE IF NOT EXISTS chathistory("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "sendid INT, "
                      "recvid INT, "
                      "msgcontent TEXT, "
                      "sendtime INT, "
                      "msgtype INT);";
    char *err = nullptr;
    rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        if (err) { cerr << err << endl; sqlite3_free(err); }
        sqlite3_close(db);
        return false;
    }
    sqlite3_close(db);
    return true;
}

// 聊天记录插入
bool insertChatHistory(const string &content
                        , int &sendId, int &receiveId, EnMsgType msgType)
{
    sqlite3 *db;
    int rc = sqlite3_open("./client_chat.db", &db);
    if (rc != SQLITE_OK)
    {
        cerr << "cannot open database: " << sqlite3_errmsg(db) << endl;
        if (db) sqlite3_close(db);
        return false;
    }

    // insert into specific columns (exclude auto id)
    char sql[2048] = {0};
    unsigned long now = (unsigned long)time(nullptr);
    // use parameterized API would be better, but keep simple sprintf for now
    snprintf(sql, sizeof(sql), "INSERT INTO chathistory(sendid, recvid, msgcontent, sendtime, msgtype) VALUES(%d, %d, '%s', %lu, %d);",
             sendId, receiveId, content.c_str(), now, (int)msgType);

    char *err = nullptr;
    rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        if (err) { cerr << err << endl; sqlite3_free(err); }
        sqlite3_close(db);
        return false;
    }
    sqlite3_close(db);
    return true;
}

// 消息加密传输
void msgencrypt(const string &msg)
{

}