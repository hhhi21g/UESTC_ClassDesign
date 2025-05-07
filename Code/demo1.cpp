#include <stdio.h>
#include <winsock2.h> // 引入Windows平台下的网络通信API：Winsock2
#pragma comment(lib, "ws2_32.lib")
#include <pthread.h>  // 引入POSIX线程库，用于多线程编程

#define PORT 9090
#define BUFFER_SIZE 1024

void send_html(SOCKET s, const char *filename);
void *handle_client(void *client_socket);
void send_error(SOCKET s, const char *error_message);
void parse_request(char *request, char *method, char *path);
void handle_post(SOCKET client, const char *body);
void forward_to_backend(SOCKET client_sock, const char *original_request, int total_len);

// 错误处理函数
int merror(SOCKET redata, SOCKET error, const char *showinfo)
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
    SOCK_STREAM: TCP协议;
    IPPROTO_TCP: TCP协议
    */
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // 创建socket
    merror(server, INVALID_SOCKET, "create socket fail");

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;         // 和创建server时相同
    server_addr.sin_port = htons(PORT);       // 端口号，网络是大端存储，pc是小段存储
    server_addr.sin_addr.s_addr = INADDR_ANY; // 监听任意地址

    // bind(),把socket绑定到9090端口
    isok = bind(server, (struct sockaddr *)&server_addr, sizeof(server_addr));
    merror(isok, SOCKET_ERROR, "bind fail");

    isok = listen(server, 5); // 开始监听，最多5个连接排队
    merror(isok, SOCKET_ERROR, "listen fail");

    struct sockaddr_in client_addr; // 客户端
    int client_len = sizeof(client_addr);
    while (1)
    {
        // accept():阻塞函数，没有客户端连接保持等待
        SOCKET client = accept(server, (struct sockaddr *)&client_addr, &client_len); // 谁连接了
        merror(client, INVALID_SOCKET, "accept fail");

        pthread_t tid;
        SOCKET *pclient = (SOCKET *)malloc(sizeof(SOCKET));
        *pclient = client;
        pthread_create(&tid, NULL, handle_client, (void *)pclient); // 为每个客户端创建一个新线程
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
    free(client_socket);
    char buffer[8192] = "";
    int total = 0, bytes = 0;

    while ((bytes = recv(client, buffer + total, sizeof(buffer) - total - 1, 0)) > 0)
    {
        total += bytes;
        buffer[total] = '\0';
        if (strstr(buffer, "\r\n\r\n"))
            break;
    }
    if (total == 0)
    {
        closesocket(client);
        return NULL;
    }

    int content_length = 0;
    char *cl = strstr(buffer, "Content-Length:");
    if (cl != NULL)
    {
        sscanf(cl, "Content-Length: %d", &content_length);
    }

    char *body_start = strstr(buffer, "\r\n\r\n");
    int header_len = 0;
    if (body_start != NULL)
    {
        body_start += 4;
        header_len = body_start - buffer;
    }

    int body_received = total - header_len;
    while (body_received < content_length && total < sizeof(buffer) - 1)
    {
        bytes = recv(client, buffer + total, sizeof(buffer) - total - 1, 0);
        if (bytes <= 0)
            break;
        total += bytes;
        body_received += bytes;
        buffer[total] = '\0';
    }

    if (body_received < content_length)
    {
        printf("请求体不完整，拒绝转发\n");
        send_error(client, "400 Bad Request");
        closesocket(client);
        return NULL;
    }

    printf("请求内容：\n%s\n", buffer);

    // 转发到 Java 后端（localhost:8080）
    forward_to_backend(client, buffer, total);

    closesocket(client);
    return NULL;
}

void handle_post(SOCKET client, const char *body)
{
    // 解析POST请求体shuju
    printf("POST data received: %s\n", body);

    // 返回确认消息给客户端
    const char *response = "HTTP/1.1 200 OK\r\nContent-Type:text/html\r\n\r\n<h1>POST data received successfully!</h1>";
    send(client, response, strlen(response), 0);
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

void forward_to_backend(SOCKET client_sock, const char *original_request, int total_len)
{
    SOCKET backend_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in backend_addr;
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(8080);                       // Java Web 项目监听端口
    backend_addr.sin_addr.s_addr = inet_addr("212.129.223.4"); // 远程服务器 IP

    if (connect(backend_sock, (struct sockaddr *)&backend_addr, sizeof(backend_addr)) < 0)
    {
        perror("连接 Java 后端失败");
        closesocket(backend_sock);
        return;
    }

    //  解析方法与路径
    char method[8], path[1024];
    sscanf(original_request, "%s %s", method, path);

    //  找出第一行结束位置（\r\n）
    const char *first_line_end = strstr(original_request, "\r\n");
    if (!first_line_end)
    {
        send_error(client_sock, "400 Bad Request");
        closesocket(backend_sock);
        return;
    }

    //  计算 header+body 的剩余部分长度
    int remaining_len = total_len - (first_line_end + 2 - original_request);

    //  构造新的 request line（替换路径）
    char request_line[256];
    if (strcmp(path, "/upload") == 0 || strcmp(path, "/delete") == 0)
        sprintf(request_line, "%s /webTest%s HTTP/1.1\r\n", method, path);
    else
        sprintf(request_line, "%s %s HTTP/1.1\r\n", method, path); // 保留原路径

    // 拼接最终请求（新 request line + 剩余）
    char modified_request[8192];
    int new_len = sprintf(modified_request, "%s%.*s",
                          request_line,
                          remaining_len,
                          first_line_end + 2);

    // 转发请求
    send(backend_sock, modified_request, new_len, 0);

    printf("转发内容如下：\n%s\n", modified_request);

    // 接收响应并转发给浏览器
    char response_buffer[8192];
    int bytes;
    while ((bytes = recv(backend_sock, response_buffer, sizeof(response_buffer), 0)) > 0)
    {
        send(client_sock, response_buffer, bytes, 0);
    }

    closesocket(backend_sock);
}
