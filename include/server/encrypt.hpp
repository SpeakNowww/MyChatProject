#ifndef ENCRYPT_H
#define ENCRYPT_H
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "public.hpp"

using namespace std;

// AES256-CBC加密 json->二进制
string aes256CBCEncrypt(const std::string& plaintext, const unsigned char* key);

// AES256-CBC解密  二进制->json
string aes256CBCDecrypt(const string& ciphertext, const unsigned char* key);

// 生成全局唯一消息id
uint32_t generateUniqueMsgId(const string &clientId);

// ack包
void buildACKPacket(uint32_t &msgId, uint32_t &srcMsgType
                    , char* sendBuf, int &bufLength);

// 检验ack有效性
bool isACKValid(uint32_t srcMsgId, const MsgHeader &ackHeader);



#endif