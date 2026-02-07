#include "nlohmann/json.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>
using namespace std;
using json = nlohmann::json;

string func1()
{
    json js;
    js["id"] = {1, 2, 3, 4, 5};
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    //js["msg"] = "hello";
    // cout << js << endl;
    // cout << js.dump() << endl;
    //js["msg"]["zhang san"] = "hello, li si";
    //js["msg"]["li si"] = "hello, zhang san";
    js["msg"] = {{"li si", "hello zhang san"}, {"zhang san", "hello li si"}};
    return js.dump(4);
}
string func2()
{
    json js;
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);   
    vec.push_back(5);

    js["list"] = vec;
    
    map<int, string> m;
    m.insert({1, "huangshan"});
    m.insert({2, "guilin"});
    m.insert({3, "zhangjiajie"});
    js["paths"] = m;
    return js.dump(4);
}
int main()
{
    string recv = func2();
    //func2();
    json jsbuf = json::parse(recv);
    // cout << "from: " << jsbuf["from"] << endl;
    // cout << "to: " << jsbuf["to"] << endl;
    // cout << "msg_type: " << jsbuf["msg_type"] << endl;
    // cout << jsbuf["msg"] << endl;
    // cout << jsbuf["id"] << endl;
    vector<int> vec = jsbuf["list"].get<vector<int>>();
    for(auto &e : vec)
    {
        cout << e << " ";
    }
    cout << endl;
    map<int, string> m = jsbuf["paths"].get<map<int, string>>();
    for(auto &e : m)
    {
        cout << e.first << " : " << e.second << endl;
    }
    return 0;
}
