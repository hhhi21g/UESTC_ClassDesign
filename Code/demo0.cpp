#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <pthread.h>

// 服务器监听的端口号，服务器将在本地机器的对应端口上等待客户端的连接
#define PORT 8081
// 每次接收数据时缓冲区的大小
#define BUFFER_SIZE 1024

// 处理客户端请求的线程函数
void *handle_client(void *arg)
{ // 在多线程编程中，void*能够让线程函数处理各种类型的数据，而不仅限于某个特定类型
    int client_sock = *(int *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_received; // 操作成功返回成功处理的字节数，失败则返回负值

    // 接收客户端请求
    /*
        int recv(SOCKET s, char *buf, int len, int flags);
        从已连接的套接字s中接收数据，将接收到的数据存储到缓冲区buf中；
        len:缓冲区大小;
        flags: 0表示使用默认的阻塞模式，直到接收到数据才返回
    */
    bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
    if (bytes_received < 0)
    {
        perror("Failed to receive data from client");
        closesocket(client_sock);
        return NULL;
    }

    buffer[bytes_received] = '\0';
    printf("Received request: %s\n", buffer);

    if (strncmp(buffer, "GET", 3) == 0)
    { // 判断前3个字符是否等于GET，是则=0
        // 返回简单的HTML页面
        const char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Hello, World!</h1>";
        // 通过send将HTTP相应发送回客户端
        send(client_sock, response, strlen(response), 0);
    }
    else
    {
        // 其他请求方法，返回错误
        const char *error_response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_sock, error_response, strlen(error_response), 0);
    }

    // 关闭连接
    closesocket(client_sock);
    return NULL;
}

int main()
{
    // 套接字类型：表示一个网络连接或者通信的端点，用来发送、接收数据
    SOCKET server_sock, client_sock;
    /*
    结构体sockaddr_int: 用于存储与网络地址相关的信息
    sin_family: 地址族，通常为AF_INET，表示IPv4地址
    sin_port: 端口号，使用htons函数将端口号转换为网络字节序
    sin_addr: IP地址，使用函数转换为网络字节序
    */
    struct sockaddr_in server_addr, client_addr; // 服务器和客户端的网络地址
    /*
    用于存储套接字地址结构的大小，通常作为accept函数的参数，用来指定客户端地址结构的大小
    */
    socklen_t client_len = sizeof(client_addr);
    /*
    用于表示线程标识符的类型：由操作系统分配，用于存储创建的线程ID
    */
    pthread_t thread_id;

    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    { // 初始化失败
        perror("WSAStartup failed");
        exit(1);
    }

    /*
    SOCK_STREAM: 表示该套接字是一个流套接字，用于基于TCP的连接
    SOCK_DGRAM：表示数据报套接字，用于UDP
    */
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0)
    { // 创建失败
        perror("Failed to create socket");
        exit(1);
    }
    // 初始化服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    // closesocket(server_sock);

    // 服务器将绑定到所有可用的网络接口上，接受来自任何客户端的请求
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 绑定地址和端口
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        closesocket(server_sock);
        exit(1);
    }

    // 监听客户端连接，5为最大等待连接队列的大小
    if (listen(server_sock, 5) < 0)
    {
        perror("Listen failed");
        closesocket(server_sock);
        exit(1);
    }

    printf("Server listening on port %d...\n", PORT);

    // 接受客户端连接并创建线程处理
    while (1)
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0)
        {
            perror("Accept failed");
            continue;
        }

        printf("Client connected\n");

        // 为每个客户端连接创建一个线程
        if (pthread_create(&thread_id, NULL, handle_client, &client_sock) != 0)
        {
            perror("Thread creation failed");
            WSACleanup();
            return 0;
        }
        else
        {
            pthread_detach(thread_id); // 自动回收线程资源
        }
    }

    closesocket(server_sock);
    return 0;
}