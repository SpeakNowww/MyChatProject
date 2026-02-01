#include "friendmodel.hpp"
#include "db.h"

// 添加好友
void friendModel::insert(int userid, int friendid)
{
    // 组装sql语句
    char sql1[1024] = {0};
    char sql2[1024] = {0};
    sprintf(sql1, "insert into friend values(%d, %d)", userid, friendid);
    sprintf(sql2, "insert into friend values(%d, %d)", friendid, userid);
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql1);
        mysql.update(sql2);
    }
    return;
}

// 返回用户好友列表（这里好友列表其实应该存本地）
vector<User> friendModel::query(int userid)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql
    , "select a.id, a.name, a.state from user a inner join friend b on b.friendid = a.id where b.userid = %d", userid);

    MySQL mysql;
    vector<User> vec;
    if(mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if(res)
        {
            MYSQL_ROW row;
            // 把用户的所有离线消息放入vector返回
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}