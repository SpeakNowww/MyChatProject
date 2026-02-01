#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis() : _publish_context(nullptr), _subscribe_context(nullptr) {}

Redis::~Redis() 
{
    if(_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }

    if(_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
    } 
}

// 连接redis服务器
bool Redis::connect()
{
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (_publish_context == nullptr)
    {
        cerr << "failed to connect to redis" << endl;
        return false;
    }

    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if (_subscribe_context == nullptr)
    {
        cerr << "failed to connect to redis" << endl;
        return false;
    }

    // 单独线程中监听通道消息，因为订阅会阻塞线程，防止阻塞主线程
    thread t([&]()
    {
        observer_channel_message();
    }   
    );
    t.detach();

    cout << "successfully connected to redis" << endl;
    return true;
}

// 向redis指定的通道channel publish消息
bool Redis::publish(int channel, string message)
{
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s"
        , channel, message.c_str());
    if (reply == nullptr)
    {
        cerr << "failed to publish command" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道channel subscribe消息
bool Redis::subscribe(int channel)
{
    // subscribe命令会造成线程阻塞等待消息，这里只做订阅通道，不接受通道消息
    // 通道消息接收在专门的observer——channel——message函数的独立线程
    // 这里仅仅把命令组装好后放到缓存区
    // 如果不这样做，而是redisCommand的话，就是组装+发送+等回复，就阻塞了
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context, "SUBSCRIBE %d", channel))
    {
        cerr << "falied to subscribe command" << endl;
        return false;
    }

    // redisbufferwrite可以循环发送缓冲区，直到缓冲区数据发送完毕，done置1
    int done = 0;
    while(!done)
    {
        // 这里再发送命令
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr << "falied to subscribe command" << endl;
            return false;
        }
    }
    return true;
}
 
// 向redis指定的通道channel unsubscribe消息
bool Redis::unsubscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context, "UNSUBSCRIBE %d", channel))
    {
        cerr << "falied to unsubscribe command" << endl;
        return false;
    }

    // redisbufferwrite可以循环发送缓冲区，直到缓冲区数据发送完毕，done置1
    int done = 0;
    while(!done)
    {
        // 这里再发送命令
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr << "falied to unsubscribe command" << endl;
            return false;
        }
    }
    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    while (redisGetReply(this->_subscribe_context, (void **)&reply) == REDIS_OK)
    {
        // 订阅收到的消息是一个带三个元素的数组,下标分别为012 1:channel. 2:message
        if (reply != nullptr && reply->element[2] != nullptr
        && reply->element[2]->str != nullptr)
        {
            // 给业务层上报通道消息
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);
    }

    cerr << ">>>>>>>>>>>>>>observer_channel_message quit<<<<<<<<<<<<<" << endl;
}

// 初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int, string)> fn)
{
    this->_notify_message_handler = fn;
}
