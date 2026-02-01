#include "encrypt.hpp"
using namespace std;

// // AES256-CBC加密 json->二进制
// string aes256CBCEncrypt(const string &plainText)
// {
//     // string cipherText;
//     AES_KEY key;
//     if (AES_set_encrypt_key(MY_AES_KEY, 256, &key) < 0) 
//     {
//         return "";
//     }

//     unsigned char iv[16];
//     RAND_bytes(iv, 16);

//     int paddingLength = 16 - (plainText.size() % 16);
//     string plainPadded = plainText;
//     plainPadded.append(paddingLength, (char)paddingLength);

//     vector<unsigned char> buf(plainPadded.size());
//     unsigned char ivCopy[16];
//     memcpy(ivCopy, iv, 16);
//     // for(size_t i = 0; i < plainPadded.size(); i+= 16)
//     // {
//     //     AES_cbc_encrypt((unsigned char*)&plainPadded[i], buf + i
//     //     , 16, &key, (unsigned char*)MY_AES_IV, AES_ENCRYPT);
//     // }
//     AES_cbc_encrypt((unsigned char*)plainPadded.data(), buf.data()
//     , plainPadded.size(), &key, ivCopy, AES_ENCRYPT);

//     string cipherText((char*)iv, 16);

//     cipherText.append((char*)buf.data(), buf.size());
//     return cipherText;
// }

string aes256CBCEncrypt(const string& plaintext, const unsigned char* key) 
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) 
    {
        cerr << "上下文创建失败" << endl;
        return ""; 
    }

    // 生成随机 IV
    try
    {
        unsigned char iv[EVP_MAX_IV_LENGTH];
        if (RAND_bytes(iv, EVP_CIPHER_iv_length(EVP_aes_256_cbc())) != 1) 
        {
            EVP_CIPHER_CTX_free(ctx);
            cerr << "随机IV生成失败" << endl;
            return "";
        }
    
        // 初始化加密操作
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) 
        {
            EVP_CIPHER_CTX_free(ctx);
            cerr << "初始化加密失败" << endl;
            return "";
        }
    
        std::vector<unsigned char> ciphertext(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
        int outLen1 = 0, outLen2 = 0;
    
        // 处理明文数据
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &outLen1, 
                             reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size()) != 1) 
        {
            EVP_CIPHER_CTX_free(ctx);
            cerr << "加密失败" << endl;
            return "";
        }
    
        // 完成加密最后一块16B，处理填充
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + outLen1, &outLen2) != 1) 
        {
            EVP_CIPHER_CTX_free(ctx);
            cerr << "密文填充失败" << endl;
            return "";
        }
    
        int ciphertext_len = outLen1 + outLen2;
        EVP_CIPHER_CTX_free(ctx);
    
        // 将IV和密文拼接在一起返回，解密时需要先分离出IV
        std::string output;
        output.append(reinterpret_cast<const char*>(iv), EVP_CIPHER_iv_length(EVP_aes_256_cbc()));
        output.append(reinterpret_cast<const char*>(ciphertext.data()), ciphertext_len);
        return output;
    }
    catch(...)
    {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
    
}


string aes256CBCDecrypt(const string& ciphertext, const unsigned char* key) 
{
    // 检查输入长度是否足够包含IV
    if (ciphertext.size() <= EVP_CIPHER_iv_length(EVP_aes_256_cbc())) 
    {
        throw std::runtime_error("Ciphertext too short");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        throw std::runtime_error("Failed to create cipher context");
    }

    std::string plaintext;
    try 
    {
        // 提取IV16字节
        const unsigned char* iv = reinterpret_cast<const unsigned char*>(ciphertext.data());
        // 实际密文部分
        const unsigned char* encrypted_data = iv + EVP_CIPHER_iv_length(EVP_aes_256_cbc());
        size_t encrypted_len = ciphertext.size() - EVP_CIPHER_iv_length(EVP_aes_256_cbc());

        // 初始化解密操作
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) 
        {
            throw std::runtime_error("Decrypt init failed");
        }

        // 分配输出缓冲区（可能包含填充）
        std::vector<unsigned char> plain_buffer(encrypted_len + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
        int out_len1 = 0, out_len2 = 0;

        // 处理密文数据
        if (EVP_DecryptUpdate(ctx, plain_buffer.data(), &out_len1, encrypted_data, encrypted_len) != 1) 
        {
            throw std::runtime_error("Decrypt update failed");
        }

        // 完成解密，处理填充
        if (EVP_DecryptFinal_ex(ctx, plain_buffer.data() + out_len1, &out_len2) != 1) 
        {
            throw std::runtime_error("Decrypt final failed - possibly incorrect key or corrupted data");
        }

        int total_len = out_len1 + out_len2;
        plaintext.assign(reinterpret_cast<const char*>(plain_buffer.data()), total_len);

        EVP_CIPHER_CTX_free(ctx);
    } 
    catch (...) 
    {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }

    return plaintext;
}

// // AES256-CBC解密  二进制->json
// string aes256CBCDecrypt(const string& cipherText)
// {
//     if (cipherText.size() < 16) return "";
//     // string plainText;
//     unsigned char iv[16];
//     memcpy(iv, cipherText.data(), 16);
//     string actualCipher = cipherText.substr(16);
//     if(actualCipher.size() % 16 != 0) return "";

//     AES_KEY key;
//     if (AES_set_decrypt_key(MY_AES_KEY, 256, &key) < 0)
//     {
//         return "";
//     }

//     vector<unsigned char> buf(actualCipher.size());
//     unsigned char ivCopy[16];
//     memcpy(ivCopy, iv, 16);

//     // for(size_t i = 0; i < cipherText.size(); i += 16)
//     // {
//     //     AES_cbc_encrypt((unsigned char*)&cipherText[i], buf + i
//     //     , 16, &key, (unsigned char*)MY_AES_IV, AES_DECRYPT);
//     // }

//     AES_cbc_encrypt((unsigned char*)actualCipher.data(), buf.data(),
//                    actualCipher.size(), &key, ivCopy, AES_DECRYPT);

//     // 块大小16字节，原始数据假如n字节，则下标从n到15的各个元素的值均为16-n
//     // 安全检查，检查填充长度1-16之间且小于缓冲区大小
//     // 最后paddingLength个字节的值必须都是paddingLength
//     int paddingLength = buf[actualCipher.size() - 1];
//     if (paddingLength < 0 || paddingLength > 16
//         || static_cast<size_t>(paddingLength) > buf.size())
//     {
//         return "";
//     }

//     for (size_t i = buf.size() - paddingLength; i < buf.size(); i++)
//     {
//         if (buf[i] != paddingLength) return "";
//     }
//     // plainText.assign((char*)buf, cipherText.size() - paddingLength);
//     return string((char*)buf.data(), buf.size() - paddingLength);
// }

// 生成全局唯一消息id
uint32_t generateUniqueMsgId(const string &clientId)
{
    uint32_t ts = (uint32_t)time(nullptr);
    uint32_t suffix = clientId.empty() ? 
    0 : stoi(clientId.substr(clientId.size() - 4)) % 10000;
    return (ts << 12) | suffix;
}

// ack包
void buildACKPacket(uint32_t &msgId, uint32_t &srcMsgType
                    , char* sendBuf, int &bufLength)
{
    MsgHeader ackHeader = {0};
    ackHeader.msgLength = sizeof(MsgHeader);
    ackHeader.msgType = ACK_MSG;
    ackHeader.msgId = msgId;
    ackHeader.isEncrypted = 0;
    memcpy(sendBuf, &ackHeader, sizeof(MsgHeader));
    bufLength = sizeof(MsgHeader);
}

// 检验ack有效性
bool isACKValid(uint32_t srcMsgId, const MsgHeader &ackHeader)
{
    return ackHeader.msgType == ACK_MSG && ackHeader.msgId == srcMsgId;
}
