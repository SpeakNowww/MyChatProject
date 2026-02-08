#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
#include <string>
#include <optional>
using namespace std;

class Redis
{
public:
    Redis();
    ~Redis();

    // 连接redis服务器
    bool connect();

    // 向redis指定的通道channel publish消息
    bool publish(int channel, string message);

    // 向redis指定的通道channel subscribe消息
    bool subscribe(int channel);
     
    // 向redis指定的通道channel unsubscribe消息
    bool unsubscribe(int channel);

    // 在独立线程中接收订阅通道中的消息
    void observer_channel_message();

    // 初始化向业务层上报通道消息的回调对象
    void init_notify_handler(function<void(int, string)> fn);

    // 读消息，不存在则返回空，转sql
    std::optional<std::string> Get(const std::string &key);

    // 写消息，传入过期时间
    bool Set(const std::string &key, const std::string &value, int expire_sec = 120);

    // 删除消息
    bool Delete(const std::string &key);

private:
    // hiredis同步上下文对象，负责publish消息
    redisContext *_publish_context;

    // hiredis同步上下文对象，负责subscribe消息
    redisContext *_subscribe_context;

    // 设置redis
    // redisContext *g_redis_ctx;

    // 回调操作，收到订阅的消息，给service层上报
    function<void(int, string)> _notify_message_handler;
};

#endif