#include <stdio.h>
#include <winsock2.h> // 引入Windows平台下的网络通信API：Winsock2
#pragma comment(lib, "ws2_32.lib")
#include <pthread.h> // 引入POSIX线程库，用于多线程编程

#define PORT 9090
#define BUFFER_SIZE 1024

// void send_html(SOCKET s, const char *filename);
void *handle_client(void *client_socket);
void send_error(SOCKET s, const char *error_message);
void parse_request(char *request, char *method, char *path);
void handle_post(SOCKET client, const char *body);
void forward_to_backend(SOCKET client_sock, const char *original_request, int total_len);

char saved_cookie[256] = "";
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

    closesocket(server); // 关闭服务器socket
    WSACleanup();        // 释放Winsock资源
    getchar();
    return 0;
}

// 解析请求行
void parse_request(char *request, char *method, char *path)
{
    // HTTP请求行格式：METHOD /path HTTP/1.1，以空格分隔
    sscanf(request, "%s %s", method, path);
}

void *handle_client(void *client_socket)
{
    SOCKET client = *(SOCKET *)client_socket;
    free(client_socket);
    char buffer[8192] = ""; // 准备一个缓冲区接收请求
    int total = 0, bytes = 0;

    while ((bytes = recv(client, buffer + total, sizeof(buffer) - total - 1, 0)) > 0)
    {
        total += bytes;
        buffer[total] = '\0';
        if (strstr(buffer, "\r\n\r\n")) // 接收到请求头结束标志
            break;
    }

    // 什么都没收到，直接关闭连接
    if (total == 0)
    {
        closesocket(client);
        return NULL;
    }

    // 找到Content-Length字段
    int content_length = 0;
    char *cl = strstr(buffer, "Content-Length:");
    if (cl != NULL)
    {
        sscanf(cl, "Content-Length: %d", &content_length);
    }

    char *body_start = strstr(buffer, "\r\n\r\n"); // body起点
    int header_len = 0;
    if (body_start != NULL)
    {
        body_start += 4;
        header_len = body_start - buffer; // header长度
    }

    // 如果body没收全，继续接收，直到收够指定长度
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

    if (body_received < content_length) // 收不全报错
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
    printf("POST data received: %s\n", body);

    // 返回确认消息给客户端
    const char *response = "HTTP/1.1 200 OK\r\nContent-Type:text/html\r\n\r\n<h1>POST data received successfully!</h1>";
    send(client, response, strlen(response), 0);
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

    // 解析请求方法和路径
    char method[8], path[1024];
    sscanf(original_request, "%s %s", method, path);

    // 找到第一行结束
    const char *first_line_end = strstr(original_request, "\r\n");
    if (!first_line_end)
    {
        send_error(client_sock, "400 Bad Request");
        closesocket(backend_sock);
        return;
    }

    // 生成新的请求路径
    char request_line[256];
    if (strcmp(path, "/upload") == 0 || strcmp(path, "/delete") == 0 || strcmp(path, "/memos") == 0)
        sprintf(request_line, "%s /webTest%s HTTP/1.1\r\n", method, path);
    else
        sprintf(request_line, "%s %s HTTP/1.1\r\n", method, path);

    // 提取 header 和 body
    char headers[4096] = "", body[4096] = "";
    const char *header_start = first_line_end + 2;
    const char *header_end = strstr(header_start, "\r\n\r\n");
    if (!header_end)
    {
        send_error(client_sock, "400 Bad Request");
        closesocket(backend_sock);
        return;
    }

    int header_len = header_end - header_start;
    strncpy(headers, header_start, header_len);
    headers[header_len] = '\0';
    int body_len = total_len - (header_end + 4 - original_request);
    memcpy(body, header_end + 4, body_len);
    if (body_len < sizeof(body))
        body[body_len] = '\0'; // null 结尾

    // sprintf(content_len_header, "Content-Length: %d\r\n", body_len);
    // 清理重复的 Host、Cookie、Content-Length
    char cleaned_headers[4096] = "";
    int has_host = 0, has_cookie = 0, has_clen = 0;

    char *line = strtok(headers, "\r\n");
    while (line != NULL)
    {
        if (strstr(line, "Host:"))
        {
            if (!has_host)
            {
                has_host = 1;
                strcat(cleaned_headers, "Host: 212.129.223.4:8080\r\n");
            }
        }
        else if (strstr(line, "Cookie:"))
        {
            if (!has_cookie && strlen(saved_cookie) > 0)
            {
                has_cookie = 1;
                char cookie_line[512];
                sprintf(cookie_line, "Cookie: %s\r\n", saved_cookie);
                strcat(cleaned_headers, cookie_line);
                printf("🍪 插入 Cookie: %s", cookie_line);
            }
        }
        else if (strstr(line, "Content-Length:"))
        {
            if (!has_clen)
            {
                has_clen = 1;
                char clen_line[128];
                sprintf(clen_line, "Content-Length: %d\r\n", body_len);
                strcat(cleaned_headers, clen_line);
            }
        }
        else
        {
            strcat(cleaned_headers, line);
            strcat(cleaned_headers, "\r\n");
        }
        line = strtok(NULL, "\r\n");
    }

    // 如果没有 Content-Length 也补上
    if (!has_clen)
    {
        char clen_line[128];
        sprintf(clen_line, "Content-Length: %d\r\n", (int)strlen(body));
        strcat(cleaned_headers, clen_line);
    }

    // 如果没有 Cookie 也补上
    if (!has_cookie && strlen(saved_cookie) > 0)
    {
        char cookie_line[512];
        sprintf(cookie_line, "Cookie: %s\r\n", saved_cookie);
        strcat(cleaned_headers, cookie_line);
        printf("🍪 插入 Cookie: %s", cookie_line);
    }

    // 拼接请求
    char modified_request[8192];
    int new_len = sprintf(modified_request, "%s%s\r\n\r\n%s",
                          request_line,
                          cleaned_headers,
                          body);

    printf("📤 转发内容如下：\n%s\n", modified_request);
    send(backend_sock, modified_request, new_len, 0);

    // 接收后端响应并转发
    char response_buffer[8192];
    int bytes;
    while ((bytes = recv(backend_sock, response_buffer, sizeof(response_buffer), 0)) > 0)
    {
        // response_buffer[bytes] = '\0';

        // 提取并保存 JSESSIONID
        char *set_cookie = strstr(response_buffer, "Set-Cookie:");
        if (set_cookie)
        {
            char *jsid_start = strstr(set_cookie, "JSESSIONID=");
            if (jsid_start)
            {
                char *jsid_end = strchr(jsid_start, ';');
                if (jsid_end)
                    *jsid_end = '\0';
                strncpy(saved_cookie, jsid_start, sizeof(saved_cookie) - 1);
                saved_cookie[sizeof(saved_cookie) - 1] = '\0';
                printf("✅ 已保存 Cookie: %s\n", saved_cookie);
            }
        }

        // 转发响应
        send(client_sock, response_buffer, bytes, 0);
    }

    closesocket(backend_sock);
}
