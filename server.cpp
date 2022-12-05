#include <cstdio>
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <set>
#include <mutex>
#include <queue>
#include <map>
#include "defs.h"

using namespace std;

mutex client_mutex;
map<int, SOCKET> clients;
int client_id;
enum UserOp {
    Exit = 66
};
struct Msg {
    UserOp type;
    string data;
};
mutex msg_mutex;
queue<Msg> msg_q;

vector<string> Split(const string &s, const string &delim) {
    vector<string> res;
    int last = 0;
    int index = s.find_first_of(delim, last);
    while (index != string::npos) {
        res.push_back(s.substr(last, index - last));
        last = index + 1;
        index = s.find_first_of(delim, last);
    }
    if (last < s.size()) {
        res.push_back(s.substr(last));
    }
    return res;
}

void Send(int state, const map<string, string> &headers, const string &content, SOCKET client_s) { //发送html响应
    string buf;
    buf += "HTTP/2 " + to_string(state) + "\n"; //状态码
    for (auto o: headers) { //响应头
        buf += o.first + ": " + o.second + "\n";
    }
    buf += "Content-Length: " + to_string(content.size()) + "\n"; //内容长度
    buf += "\n";
    buf += content; //内容
    send(client_s, buf.c_str(), buf.size(), 0); //发送
}

string GetContentType(const string &file_path) {
    string suffix = file_path.substr(file_path.find_last_of('.') + 1); //获取文件后缀
    if (suffix == "html") { //如果是html文件
        return "text/html";
    } else if (suffix == "jpg") { //如果是jpg文件
        return "image/jpeg";
    } else if (suffix == "txt") { //如果是txt文件
        return "text/plain";
    }
    return "text/plain";
}

string GetPath(const string &URL) { //重定向文件
    string filename = URL.substr(URL.find_last_of('/') + 1); //获取文件名
    string t = GetContentType(filename); //获取文件类型
    if (t == "text/html") { //如果是网页文件
        return prefix + "html/" + filename;
    } else if (t == "image/jpeg") { //如果是图片文件
        return prefix + "img/" + filename;
    }
    return prefix + "txt/" + filename; //如果是其他文件
}

void GetHandler(SOCKET client_s, const string &URL, const map<string, string> &headers, const string &content) {
    map<string, string> res_headers; //响应头
    string path = GetPath(URL); //获取文件路径
    FILE *fp = fopen(path.c_str(), "rb"); //打开文件
    if (fp == nullptr) { //如果文件不存在
        Send(404, {}, "404 Not Found!", client_s);
        return;
    }
    fseek(fp, 0, SEEK_END); //定位到文件末尾
    int size = ftell(fp); //获取文件大小
    res_headers["Content-Type"] = GetContentType(path); //设置内容类型
    fseek(fp, 0, SEEK_SET);
    char *buf = new char[size];
    fread(buf, 1, size, fp); //读取文件内容
    Send(200, res_headers, string(buf, size), client_s); //发送响应
    delete[] buf;
    fclose(fp); //关闭文件
}

void PostHandler(SOCKET client_s, const string &URL, const map<string, string> &headers,const string &content) {
    map<string, string> res_headers;
    string filename = URL.substr(URL.find_last_of('/') + 1);
    if (filename == "dopost") {
        vector<string> form_strs = Split(content, "&");
        map<string, string> form_data;
        for (auto &o: form_strs) {
            vector<string> kv = Split(o, "=");
            form_data[kv[0]] = kv[1];
        }
        if (form_data["login"] == "3200104495" && form_data["pass"] == "4495") {
            Send(200, res_headers, "Login Success!", client_s);
        } else {
            Send(200, res_headers, "Login Failed!", client_s);
        }
    } else {
        Send(404, res_headers, "404 Not Found!", client_s);
    }
}

void Receiver(SOCKET client_s) {
    client_mutex.lock();
    int cur_id = client_id;
    clients[client_id++] = client_s;
    client_mutex.unlock();
    sockaddr_in addr;
    int len = sizeof(addr);
    getpeername(client_s, (sockaddr *) &addr, &len);
    cout << "Client " << cur_id << ": " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " connected" << endl;
    char buf[buf_size];
    string data;
    char *buf_begin, *buf_end;
    buf_begin = buf_end = buf;
    Stage cur = MethodStage;
    OpType method;
    string method_str;
    map<string, string> headers;
    string content;
    string URL;
    int remain;
    bool running = true;
    while (running) {
        int sz = recv(client_s, buf, buf_size, 0); //接收数据
        if (sz <= 0) {
            if (!sz || WSAGetLastError() == WSAECONNRESET) { //连接关闭
                cout << "Client " << cur_id << " disconnected" << endl;
            } else {
                cout << "client " << cur_id << " error:" << WSAGetLastError() << endl;
            }
            break;
        } //错误处理
        if (sz > 0) {
            buf_begin = buf; //设置缓冲区起始位置
            buf_end = buf + sz; //设置缓冲区结束位置
            while (buf_begin != buf_end) {
                if (!running) break;
                switch (cur) {
                    case MethodStage: { //解析请求方法
                        if (*buf_begin != '\n' && *buf_begin != '\r') { //如果不是回车换行符
                            method_str += *buf_begin;
                        } else if (*buf_begin == '\n') { //如果是换行符
                            cur = HeaderStage; //请求方法解析完毕，进入请求头解析阶段
                            vector<string> method_vec = Split(method_str, " ");
                            if (method_vec[0] == "GET") { //解析请求方法
                                method = Get;
                            } else if (method_vec[0] == "POST") {
                                method = Post;
                            }
                            URL = method_vec[1]; //解析URL
                            method_str.clear();
                        }
                        break;
                    }
                    case HeaderStage: { //解析请求头
                        if (*buf_begin != '\n' && *buf_begin != '\r') { //如果不是回车换行符
                            data += *buf_begin;
                        } else if (*buf_begin == '\n') {
                            if (data.empty()) { //如果连续两个换行符，说明请求头解析完毕
                                cur = ContentStage;
                                if (headers.count("Content-Length")) { //如果有Content-Length字段
                                    remain = stoi(headers["Content-Length"]);
                                } else { //否则没有请求体
                                    remain = 0;
                                }
                            } else { //解析请求头中的一行
                                int pos = data.find(':');
                                headers[data.substr(0, pos)] = data.substr(pos + 1);
                                data.clear();
                            }
                        }
                        break;
                    }
                    case ContentStage: { //解析请求内容
                        content += *buf_begin;
                        remain--;
                        break;
                    }
                }
                if (cur == ContentStage && !remain) { //请求解析完毕
                    cur = MethodStage;
                    if (method == Get) {
                        GetHandler(client_s, URL, headers, content);
                    } else if (method == Post) {
                        PostHandler(client_s, URL, headers, content);
                        running = false; //处理完后退出
                    }
                    headers.clear();
                    content.clear();
                }
                ++buf_begin;
            }
        }
    }
    client_mutex.lock();
    closesocket(client_s);
    clients.erase(cur_id);
    client_mutex.unlock();
    closesocket(client_s);
}

void UserHandler() {
    string op;
    Msg temp;
    while (1) {
        cin >> op;
        if (op == "exit") {
            temp.type = Exit;
            msg_mutex.lock();
            msg_q.push(temp);
            msg_mutex.unlock();
        }
    }
}

int main(int argc, char *argv[]) {
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        return 0;
    }

    SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (slisten == INVALID_SOCKET) {
        printf("socket error !");
        return 0;
    }
    cout << "socket created" << endl;
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(4495);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    if (bind(slisten, (LPSOCKADDR) &sin, sizeof(sin)) == SOCKET_ERROR) {
        printf("bind error !");
    }
    cout << "bind success" << endl;
    if (listen(slisten, 5) == SOCKET_ERROR) {
        printf("listen error !");
        return 0;
    }
    cout << "listen success" << endl;
    u_long a = 1;
    ioctlsocket(slisten, FIONBIO, &a);

    SOCKET sClient;
    sockaddr_in remoteAddr;
    int nAddrlen = sizeof(remoteAddr);
    fflush(stdout);
    bool run = true;
    thread user_handler(UserHandler);
    user_handler.detach();
    while (run) {
        if (!msg_q.empty()) { //查看消息队列是否为空
            msg_mutex.lock(); //加锁
            Msg temp = msg_q.front(); //取出队首元素
            msg_q.pop(); //删除队首元素
            msg_mutex.unlock(); //解锁
            switch (temp.type) { //判断消息类型
                case Exit: { //如果是退出消息
                    client_mutex.lock();
                    for (auto o: clients) {
                        closesocket(o.second);
                    }
                    client_mutex.unlock();
                    run = false;
                    break;
                }
            }
        }
        sClient = accept(slisten, (SOCKADDR *) &remoteAddr, &nAddrlen); //接受客户端连接
        if (sClient == INVALID_SOCKET) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) { //如果原因是非阻塞，继续循环
                continue;
            }
            printf("accept error !");
            continue;
        }
        a = 0;
        ioctlsocket(sClient, FIONBIO, &a); //设置为阻塞模式
        thread t(Receiver, sClient); //创建线程
        t.detach(); //分离线程
    }

    closesocket(slisten);
    WSACleanup();
    return 0;
}
