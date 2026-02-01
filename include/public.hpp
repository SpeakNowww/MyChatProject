#ifndef PUBLIC_H
#define PUBLIC_H
#include <cstdint>
/*server和client共用的头文件*/
enum EnMsgType
{
    LOGIN_MSG = 1,
    LOGOUT_MSG,
    LOGIN_MSG_ACK,
    REG_MSG,
    REG_MSG_ACK, // 注册响应消息
    ONE_CHAT_MSG, // 点对点聊天消息
    ADD_FRIEND_MSG,

    CREATE_GROUP_MSG,
    ADD_GROUP_MSG,
    GROUP_CHAT_MSG,
    CREATE_GROUP_MSG_ACK,
    ADD_GROUP_MSG_ACK,

    // ack
    ACK_MSG = 102
};

typedef struct MsgHeader
{
    uint32_t msgLength; // 总长度（消息头+加密正文）
    uint32_t msgType;
    uint32_t msgId;
    uint32_t isEncrypted;
}MsgHeader;

constexpr unsigned char MY_AES_KEY[] = "1234567890ABCDEF1234567890ABCDEF";
// constexpr unsigned char MY_AES_IV[] = "1234567890ABCDEF";

const int RETRY_MAX = 3;

#endif