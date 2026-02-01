#ifndef USERMODEL_H
#define USERMODEL_H
#include "user.hpp"

// user表数据操作类
class UserModel
{
public:
    bool insert(User &user);

    // 根据用户id查询用户信息
    User query(int id);

    // 更新用户信息
    bool updateState(User user);

    //重置用户状态
    void resetState();
};

#endif