#include <stdio.h>
#include <winsock2.h> // 包含网络编程的头文件，引入静态库
#pragma comment(lib, "ws2_32.lib")
#include <pthread.h>

#define PORT 80
#define BUFFER_SIZE 1024

void send_html(SOCKET s, const char *filename);
void *handle_client(void *client_socket);
void send_error(SOCKET s, const char *error_message);
void parse_request(char *request, char *method, char *path);

int merror(int redata, int error, const char *showinfo)
{
    if (redata == error)
    {
        perror(showinfo); // 打印错误信息
        getchar();
        return -1;
    }
    return 0;
}
int main()
{
    printf("Web Server started...\n");
    WSAData wsdata;
    int isok = WSAStartup(MAKEWORD(2, 2), &wsdata); // 确定socket版本信息
    merror(isok, WSAEINVAL, "ask socket fail");

    /*
    AF_INET: IPv4;
    SOCK_STREAM: TCP协议
    */
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // 创建socket
    merror(server, INVALID_SOCKET, "create socket fail");

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;         // 和创建server一样
    server_addr.sin_port = htons(PORT);       // 端口号，网络是大端存储，pc是小段存储
    server_addr.sin_addr.s_addr = INADDR_ANY; // 监听任意地址
    isok = bind(server, (struct sockaddr *)&server_addr, sizeof(server_addr));
    merror(isok, SOCKET_ERROR, "bind fail");

    isok = listen(server, 5); // 监听
    merror(isok, SOCKET_ERROR, "listen fail");

    struct sockaddr_in client_addr; // 客户端地址
    int client_len = sizeof(client_addr);
    while (1)
    {
        SOCKET client = accept(server, (struct sockaddr *)&client_addr, &client_len); // 谁连接了
        merror(client, INVALID_SOCKET, "accept fail");

        // char revdata[BUFFER_SIZE] = "";
        // recv(client, revdata, BUFFER_SIZE, 0); // 接收数据
        // printf("%s receive %d bytes\n", revdata, strlen(revdata));

        // char sendata[1024] = "<h1>I'm test</h1>";
        // send(client, sendata, sizeof(sendata), 0); // 发送数据
        // const char *filename = "ChatGPT.html";
        // sendhtml(client, filename);
        // closesocket(client);
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void *)&client); // 为每个客户端创建一个新线程
    }

    closesocket(server);
    WSACleanup();
    getchar();
    return 0;
}
void parse_request(char *request, char *method, char *path)
{
    sscanf(request, "%s %s", method, path); // 解析请求行
}
void *handle_client(void *client_socket)
{
    SOCKET client = *(SOCKET *)client_socket;
    char buffer[BUFFER_SIZE] = "";
    int bytes_received = recv(client, buffer, sizeof(buffer), 0);

    if (bytes_received <= 0)
    {
        closesocket(client);
        return NULL;
    }

    printf("Request: %s\n", buffer);

    char method[10], path[1024];
    parse_request(buffer, method, path); // 解析请求

    // 处理请求
    if (strcmp(method, "GET") == 0)
    {
        if (strcmp(path, "/") == 0)
        {
            send_html(client, "index.html"); // 发送首页
        }
        else
        {
            send_html(client, "404 Not Found");
        }
    }
    else if (strcmp(method, "POST") == 0)
    {
    }
    else
    {
        send_error(client, "405 Method Not Allowed");
    }
    closesocket(client);
    return NULL;
}
void send_html(SOCKET s, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        // printf("open file fail\n");
        send_error(s, "404 Not Found");
        return;
    }

    char response_header[512];
    sprintf(response_header, "HTTP/1.1 200 OK\r\nContent-Type:text/html\r\n\r\n");
    send(s, response_header, strlen(response_header), 0);

    char temp[1024] = "";
    do
    {
        fgets(temp, 1024, fp);
        send(s, temp, strlen(temp), 0); // 发送数据
    } while (!feof(fp));
    fclose(fp);
}

void send_error(SOCKET s, const char *error_message)
{
    char response_header[512];
    sprintf(response_header, "HTTP/1.1 %s\r\nContent-Type:text/html\r\n\r\n", error_message);
    send(s, response_header, strlen(response_header), 0);

    const char *error_body = "<html><body><h1>Error: %s</h1></body></html>";
    char error_page[1024];
    sprintf(error_page, error_body, error_message); // 将error_message替换到%s,输出到error_page
    send(s, error_page, strlen(error_page), 0);
}