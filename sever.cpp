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
#pragma comment(lib, "Crypt32.lib")

// �洢���пͻ����׽���
std::vector<SOCKET> client_sockets;
std::mutex client_sockets_mutex; // ������

// ������ϢժҪ��ʹ��MD5��ϣ�㷨��
std::string calculateMD5(const std::string& message) {
    HCRYPTPROV hCryptProv;
    if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        HCRYPTHASH hHash;
        if (CryptCreateHash(hCryptProv, CALG_MD5, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (BYTE*)message.c_str(), message.size(), 0)) {
                DWORD digestSize = 16; // MD5ժҪ��СΪ16�ֽ�
                BYTE digest[16];
                if (CryptGetHashParam(hHash, HP_HASHVAL, digest, &digestSize, 0)) {
                    std::string result;
                    for (int i = 0; i < digestSize; i++) {
                        char buf[3];
                        sprintf_s(buf, "%02x", digest[i]);
                        result += buf;
                    }
                    CryptDestroyHash(hHash);
                    CryptReleaseContext(hCryptProv, 0);
                    return result;
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hCryptProv, 0);
    }
    return "";
}


// �㲥��Ϣ�����пͻ��ˣ����˷�����
void broadcast_message(const std::string& message, SOCKET sender_socket) {
    std::string messageContent = message.substr(0, message.length() - 32);

    for (SOCKET client_socket : client_sockets) {
        if (client_socket != sender_socket) {
            std::string messageToSend = messageContent;
            send(client_socket, messageToSend.c_str(), messageToSend.size(), 0);
        }
    }
}

// �������ͻ��˵���Ϣ
void handle_client(SOCKET client_socket) {
    char buffer[1024];
    while (true) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            std::cout << "Client disconnected." << std::endl;
            closesocket(client_socket);

            // ʹ�û����������� client_sockets �Ĳ���
            std::lock_guard<std::mutex> lock(client_sockets_mutex);
            // �ӿͻ����׽����б����Ƴ��Ͽ����ӵĿͻ���
            client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end());
            break;
        }
        else {
            buffer[bytes_received] = '\0';
            std::string receivedMessage(buffer);

            if (receivedMessage.length() >= 33) {
                // ��ȡ�������
                std::string taskCodeStr = receivedMessage.substr(0, 1);
                int taskCode = std::stoi(taskCodeStr);
                // ��ȡ��Ϣ���ݣ�ȥ����ϣֵ��
                std::string messageContent = receivedMessage.substr(1, receivedMessage.length() - 33);
                // ��ȡ���յ��Ĺ�ϣֵ
                std::string receivedHash = receivedMessage.substr(receivedMessage.length() - 32);

                // ������Ϣ��ʵ�ʹ�ϣֵ
                std::string calculatedHash = calculateMD5(messageContent);

                if (calculatedHash == receivedHash) {
                    // ʵ�ʹ�ϣֵ����յ��Ĺ�ϣֵƥ��
                    if (taskCode == 1) {
                        // ִ����Ч����
                        std::string response = "Valid operation result: " + messageContent;
                        send(client_socket, response.c_str(), response.size(), 0);
                    }
                    else if (taskCode == 2) {
                        // ����һ���㲥��Ϣ
                        std::string response = messageContent;
                        send(client_socket, response.c_str(), response.size(), 0);
                        // �㲥��Ϣ�����пͻ��ˣ����˷�����
                        broadcast_message(messageContent, client_socket);
                    }
                    else {
                        // ������Ч�������
                        std::string response = "Unknown task code: " + messageContent;
                        send(client_socket, response.c_str(), response.size(), 0);
                    }
                }
                else {
                    // ʵ�ʹ�ϣֵ����յ��Ĺ�ϣֵ��ƥ��
                    std::string response = "Hash verification failed for message: " + messageContent;
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }
            else {
                // ��Ϣ���Ȳ��㣬�����޷���ȡ������룬������Ӧ����
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
    // ��ʼ�� Winsock �ʹ����������׽��ֵȴ���...
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock" << std::endl;
        return 1;
    }

    // ������һ���׽���
    SOCKET server_socket1 = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_address1;
    server_address1.sin_family = AF_INET;
    server_address1.sin_port = htons(521);  // ��һ���˿ں�
    server_address1.sin_addr.s_addr = INADDR_ANY;

    // �󶨺ͼ�����һ���׽���...
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

    // �����ڶ����׽���
    SOCKET server_socket2 = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_address2;
    server_address2.sin_family = AF_INET;
    server_address2.sin_port = htons(520);  // �ڶ����˿ں�
    server_address2.sin_addr.s_addr = INADDR_ANY;

    // �󶨺ͼ����ڶ����׽���...
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

    // ���������̣߳��ֱ����ڽ��ܿͻ�������
    std::thread thread1(accept_clients, server_socket1);
    std::thread thread2(accept_clients, server_socket2);

    // �ȴ��߳����
    thread1.join();
    thread2.join();

    // �رշ������׽��ֺ����� Winsock
    closesocket(server_socket1);
    closesocket(server_socket2);
    WSACleanup();
    return 0;
}
