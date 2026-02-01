#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>
using namespace std;

// 服务器ctrl+c退出以后重置user状态
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char** argv)
{
    if(argc < 3)
    {
        cerr << "command invalid! command should be like ./ChatClient ip port" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    signal(SIGINT, resetHandler);
    EventLoop loop;
    InetAddress addr("127.0.0.1", port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
}