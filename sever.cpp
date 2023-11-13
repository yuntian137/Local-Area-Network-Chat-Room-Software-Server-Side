#include <iostream>
#include <string>
#include <Winsock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <tchar.h>
#include <Windows.h>
#include <Wincrypt.h>
#pragma comment(lib, "ws2_32.lib")

// 存储所有客户端套接字
std::vector<SOCKET> client_sockets;
std::mutex client_sockets_mutex; // 互斥量

// 广播消息给所有客户端，除了发送者
void broadcast_message(const std::string& message, SOCKET sender_socket) {
    std::string messageContent = message.substr(0, message.length() - 32);

    for (SOCKET client_socket : client_sockets) {
        if (client_socket != sender_socket) {
            std::string messageToSend = messageContent;
            send(client_socket, messageToSend.c_str(), messageToSend.size(), 0);
        }
    }
}

// 处理单个客户端的消息
void handle_client(SOCKET client_socket) {
    char buffer[1024];
    while (true) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            std::cout << "Client disconnected." << std::endl;
            closesocket(client_socket);

            // 使用互斥量保护对 client_sockets 的操作
            std::lock_guard<std::mutex> lock(client_sockets_mutex);
            // 从客户端套接字列表中移除断开连接的客户端
            client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end());
            break;
        }
        else {
            buffer[bytes_received] = '\0';
            std::string receivedMessage(buffer);

            if (receivedMessage.length() >= 2) {
                // 提取任务代码
                std::string taskCodeStr = receivedMessage.substr(0, 1);
                int taskCode = std::stoi(taskCodeStr);
                // 提取消息内容（去除哈希值）
                std::string messageContent = receivedMessage.substr(1);

                // 处理任务代码
                if (taskCode == 1) {
                    // 执行有效操作
                    std::string response = "Valid operation result: " + messageContent;
                    send(client_socket, response.c_str(), response.size(), 0);
                }
                else if (taskCode == 2) {
                    // 这是一条广播消息
                    std::string response = messageContent;
                    send(client_socket, response.c_str(), response.size(), 0);
                    // 广播消息给所有客户端，除了发送者
                    broadcast_message(messageContent, client_socket);
                }
                else {
                    // 处理无效任务代码
                    std::string response = "Unknown task code: " + messageContent;
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }
            else {
                // 消息长度不足，可能无法提取任务代码，进行相应处理
                std::string response = "Invalid message format";
                send(client_socket, response.c_str(), response.size(), 0);
            }
        }
    }
}

void accept_clients(SOCKET server_socket) {
    while (true) {
        SOCKET client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed" << std::endl;
        }
        else {
            std::lock_guard<std::mutex> lock(client_sockets_mutex);
            client_sockets.push_back(client_socket);
            std::thread client_thread(handle_client, client_socket);
            client_thread.detach();
        }
    }
}

int main() {
    // 初始化 Winsock 和创建服务器套接字等代码...
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock" << std::endl;
        return 1;
    }

    // 创建第一个套接字
    SOCKET server_socket1 = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_address1;
    server_address1.sin_family = AF_INET;
    server_address1.sin_port = htons(521);  // 第一个端口号
    server_address1.sin_addr.s_addr = INADDR_ANY;

    // 绑定和监听第一个套接字...
    if (bind(server_socket1, (struct sockaddr*)&server_address1, sizeof(server_address1)) == SOCKET_ERROR) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(server_socket1);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket1, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(server_socket1);
        WSACleanup();
        return 1;
    }

    // 创建第二个套接字
    SOCKET server_socket2 = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_address2;
    server_address2.sin_family = AF_INET;
    server_address2.sin_port = htons(520);  // 第二个端口号
    server_address2.sin_addr.s_addr = INADDR_ANY;

    // 绑定和监听第二个套接字...
    if (bind(server_socket2, (struct sockaddr*)&server_address2, sizeof(server_address2)) == SOCKET_ERROR) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(server_socket2);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket2, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(server_socket2);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port 521 and 520..." << std::endl;

    // 创建两个线程，分别用于接受客户端连接
    std::thread thread1(accept_clients, server_socket1);
    std::thread thread2(accept_clients, server_socket2);

    // 等待线程完成
    thread1.join();
    thread2.join();

    // 关闭服务器套接字和清理 Winsock
    closesocket(server_socket1);
    closesocket(server_socket2);
    WSACleanup();
    return 0;
}
