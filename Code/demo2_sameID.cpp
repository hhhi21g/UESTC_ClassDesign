#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>    // posix线程库，用于多线程编程
#include <sys/socket.h> // 套接字接口定义，提供socket编程的系统调用
#include <netinet/in.h> // Internet协议族地址结构，包含sockaddr_in结构体
#include <arpa/inet.h>  // IP地址转换函数
#include <netinet/tcp.h>

#define PORT 9090 // 监听IP地址上的9090端口，等待客户端连接

void *handle_client(void *client_socket);
void send_error(int client_sock, const char *status_code, const char *message);
void forward_to_backend(int client_sock, const char *original_request, int total_len);
void handle_options_request(int client_sock);
void handle_trace_request(int client_sock, const char *original_request, int total_len);
void handle_connect_request(int client_sock);

// 错误处理函数
int merror(int redata, int error, const char *showinfo)
{
    if (redata == error)
    {
        perror(showinfo);
        return -1;
    }
    return 0;
}

int main()
{
    printf("Web Server started...\n");

    // 创建TCP套接字：使用IPv4协议，使用TCP
    int server = socket(AF_INET, SOCK_STREAM, 0);
    merror(server, -1, "create socket fail");

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;         // IPv4协议，与套接字相同
    server_addr.sin_port = htons(PORT);       // 端口号，主机字节序(通常为小端序)转为网络字节序(大端序)
    server_addr.sin_addr.s_addr = INADDR_ANY; // 监听所有IP地址上的请求
    memset(&(server_addr.sin_zero), 0, 8);    // 结构体填充位

    int reuse = 1;
    // 设置socket允许端口复用
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定地址结构体到socket,使服务器知道监听哪个端口
    int isok = bind(server, (struct sockaddr *)&server_addr, sizeof(server_addr));
    merror(isok, -1, "bind fail");

    // 开始监听，最大连接队列长度为5
    isok = listen(server, 5);
    merror(isok, -1, "listen fail");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    while (1)
    {
        // 阻塞等待客户端连接，每来一个连接就产生一个新的client
        int client = accept(server, (struct sockaddr *)&client_addr, &client_len);
        merror(client, -1, "accept fail");

        // 多线程
        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        *pclient = client;
        pthread_create(&tid, NULL, handle_client, (void *)pclient);
        pthread_detach(tid); // 自动释放线程资源，避免线程泄漏
    }

    close(server);
    return 0;
}

// 为每个客户端连接单独处理请求
void *handle_client(void *client_socket)
{
    int client = *(int *)client_socket;
    free(client_socket);

    char buffer[8192] = "";
    int total = 0, bytes = 0;

    // 接收并判断是否超长
    while ((bytes = recv(client, buffer + total, sizeof(buffer) - total - 1, 0)) > 0)
    {
        total += bytes;
        buffer[total] = '\0';

        if (total >= sizeof(buffer) - 1)
        {
            fprintf(stderr, "错误：请求超出最大缓冲区\n");
            send_error(client, "413 Payload Too Large", "请求过长");
            return NULL;
        }

        if (strstr(buffer, "\r\n\r\n"))
            break; // 收到完整 header 就跳出
    }

    if (total == 0)
    {
        close(client);
        return NULL;
    }

    // 请求格式初步验证
    if (strstr(buffer, "GET") == NULL &&
        strstr(buffer, "POST") == NULL &&
        strstr(buffer, "PUT") == NULL &&
        strstr(buffer, "DELETE") == NULL &&
        strstr(buffer, "OPTIONS") == NULL &&
        strstr(buffer, "HEAD") == NULL &&
        strstr(buffer, "CONNECT") == NULL &&
        strstr(buffer, "TRACE") == NULL)
    {
        fprintf(stderr, "错误：未检测到有效 HTTP 方法\n");
        send_error(client, "400 Bad Request", "非法请求格式");
        return NULL;
    }

    // PATCH 请求不支持
    if (strncmp(buffer, "PATCH", 5) == 0)
    {
        fprintf(stderr, "错误：收到不支持的 PATCH 方法\n");
        send_error(client, "405 Method Not Allowed", "不支持的 HTTP 方法");
        return NULL;
    }

    // 特殊方法处理
    if (strncmp(buffer, "OPTIONS", 7) == 0)
    {
        handle_options_request(client);
        close(client);
        return NULL;
    }
    if (strncmp(buffer, "TRACE", 5) == 0)
    {
        handle_trace_request(client, buffer, total);
        close(client);
        return NULL;
    }
    if (strncmp(buffer, "CONNECT", 7) == 0)
    {
        handle_connect_request(client);
        close(client);
        return NULL;
    }

    // 请求体长度检查
    int content_length = 0;
    char *cl = strstr(buffer, "Content-Length:");
    if (cl != NULL)
    {
        sscanf(cl, "Content-Length: %d", &content_length);
    }

    // 获取 body 起始位置
    char *body_start = strstr(buffer, "\r\n\r\n");
    if (body_start == NULL)
    {
        fprintf(stderr, "错误：请求头不完整，缺少 CRLF 结束符\n");
        send_error(client, "400 Bad Request", "请求头不完整");
        return NULL;
    }

    body_start += 4;
    int header_len = body_start - buffer;
    int body_received = total - header_len;

    // 如果 body 不足，继续接收
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
        fprintf(stderr, "错误：请求体长度不足（已接收 %d 字节，预期 %d 字节）\n", body_received, content_length);
        send_error(client, "400 Bad Request", "请求体长度不足");
        return NULL;
    }

    // 进入转发逻辑
    printf("请求内容：\n%s\n", buffer);
    forward_to_backend(client, buffer, total);

    close(client);
    return NULL;
}

// 3个特殊方法，不转发到后端，直接在中间人服务器上响应处理
void handle_options_request(int client_sock)
{
    // 用于跨域请求的前置检查，返回允许的跨域访问权限和方法，主要用于CORS（跨域资源共享）
    const char *response =
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Allow-Origin: http://212.129.223.4:8080\r\n"
        "Access-Control-Allow-Credentials: true\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Access-Control-Expose-Headers: Access-Control-Allow-Origin, Access-Control-Allow-Methods, Access-Control-Allow-Headers\r\n"
        "Access-Control-Max-Age: 86400\r\n"
        "\r\n"
        "OPTIONS 方法已收到并已响应（此为课程设计模拟响应）\r\n";

    send(client_sock, response, strlen(response), 0);
}

void handle_trace_request(int client_sock, const char *original_request, int total_len)
{
    // 用于回显客户端发送的原始请求
    const char *response_header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: message/http\r\n"
        "Access-Control-Allow-Origin: http://212.129.223.4:8080\r\n"
        "Access-Control-Allow-Credentials: true\r\n"
        "\r\n"
        "TRACE 方法已收到并已响应（此为课程设计模拟响应）\r\n";

    // 先发送响应头
    send(client_sock, response_header, strlen(response_header), 0);

    // 然后原样回显客户端请求内容
    send(client_sock, original_request, total_len, 0);
}

void handle_connect_request(int client_sock)
{
    // 用于建立HTTP隧道，通常用于HTTPS请求
    const char *response =
        "HTTP/1.1 200 Connection Established\r\n"
        "Access-Control-Allow-Origin: http://212.129.223.4:8080\r\n"
        "Access-Control-Allow-Credentials: true\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "CONNECT 方法已收到并已响应（此为课程设计模拟响应）\r\n";

    send(client_sock, response, strlen(response), 0);
}

// HTTP错误响应函数，快速向客户端返回一个标准的HTTP错误响应
void send_error(int client_sock, const char *status_code, const char *message)
{
    // 设置 linger 确保响应发送完成
    struct linger ling = {1, 2}; // on = 1, timeout = 2秒
    setsockopt(client_sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));

    // 构造响应
    char response[1024];
    snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %ld\r\n"
             "Access-Control-Allow-Origin: http://212.129.223.4:8080\r\n"
             "Access-Control-Allow-Credentials: true\r\n"
             "Connection: close\r\n"
             "\r\n"
             "错误：%s\r\n",
             status_code, strlen(message) + 12, message);

    // 发送响应
    int sent = 0, len = strlen(response);
    while (sent < len)
    {
        int n = send(client_sock, response + sent, len - sent, 0);
        if (n <= 0)
            break;
        sent += n;
    }

    // 关闭写通道，告知客户端“我发完了”
    shutdown(client_sock, SHUT_WR);

    // 持续接收客户端残余数据，直到对方关闭（防止 TCP Reset）
    char tmp[256];
    while (recv(client_sock, tmp, sizeof(tmp), 0) > 0)
    {
    }

    // 不再主动 close()，由 linger 控制释放
}

// 将前端网页发来的HTTP请求，重写后转发给Java后端服务器，并将响应再转发回前端浏览器
void forward_to_backend(int client_sock, const char *original_request, int total_len)
{
    // 检查是否为3个特殊请求
    if (strncmp(original_request, "OPTIONS", 7) == 0)
    {
        handle_options_request(client_sock);
        return;
    }
    if (strncmp(original_request, "TRACE", 5) == 0)
    {
        handle_trace_request(client_sock, original_request, total_len);
        return;
    }
    if (strncmp(original_request, "CONNECT", 7) == 0)
    {
        handle_connect_request(client_sock);
        return;
    }

    // 连接Java Web后端服务器
    int backend_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in backend_addr;
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(8080);
    backend_addr.sin_addr.s_addr = inet_addr("212.129.223.4");
    memset(&(backend_addr.sin_zero), 0, 8);

    if (connect(backend_sock, (struct sockaddr *)&backend_addr, sizeof(backend_addr)) < 0)
    {
        perror("连接 Java 后端失败");
        close(backend_sock);
        return;
    }

    // 请求方法+“ ”+URL
    char method[8], path[1024];
    sscanf(original_request, "%s %s", method, path);

    const char *first_line_end = strstr(original_request, "\r\n"); // 第一行结束
    if (!first_line_end)
    {
        send(client_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, 0);
        close(backend_sock);
        return;
    }

    // 提取相对路径部分
    char request_line[2048];
    if (strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0)
    {
        char *p = strstr(path + (path[4] == 's' ? 8 : 7), "/");
        if (p)
            strcpy(path, p);
    }

    if (strncmp(path, "/webTest", 8) == 0)
        // 处理是否已有/webTest前缀
        snprintf(request_line, sizeof(request_line), "%s %s HTTP/1.1\r\n", method, path);
    else if (strcmp(path, "/upload") == 0 || strcmp(path, "/delete") == 0 || strcmp(path, "/memos") == 0 || strcmp(path, "/update") == 0)
        // 是自定义的路径，则在前面补上/webTest
        snprintf(request_line, sizeof(request_line), "%s /webTest%s HTTP/1.1\r\n", method, path);
    else
        snprintf(request_line, sizeof(request_line), "%s %s HTTP/1.1\r\n", method, path);

    const char *header_start = first_line_end + 2;
    const char *header_end = strstr(header_start, "\r\n\r\n");
    if (!header_end)
    {
        send(client_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, 0);
        close(backend_sock);
        return;
    }

    int header_len = header_end - header_start;
    int body_len = total_len - (header_end + 4 - original_request);

    char headers[4096] = "", body[4096] = "";
    strncpy(headers, header_start, header_len);
    headers[header_len] = '\0';
    memcpy(body, header_end + 4, body_len);

    char cleaned_headers[4096] = "";
    const char *header_ptr = header_start;
    while (header_ptr < header_start + header_len)
    {
        const char *line_end = strstr(header_ptr, "\r\n");
        if (!line_end)
            break;

        int line_len = line_end - header_ptr;
        char line[1024] = "";
        strncpy(line, header_ptr, line_len);
        line[line_len] = '\0';

        if (strstr(line, "Host:") || strstr(line, "Content-Length:") || strstr(line, "Accept-Encoding:"))
        {
            // 跳过
        }
        else
        {
            strcat(cleaned_headers, line);
            strcat(cleaned_headers, "\r\n");
        }
        header_ptr = line_end + 2;
    }

    char clen[64];
    sprintf(clen, "Content-Length: %d\r\n", body_len);
    strcat(cleaned_headers, "Host: 212.129.223.4:8080\r\n");
    strcat(cleaned_headers, clen);
    strcat(cleaned_headers, "Accept-Encoding: identity\r\n");
    strcat(cleaned_headers, "TE: trailers\r\n");
    strcat(cleaned_headers, "Origin: http://212.129.223.4:8080\r\n");
    strcat(cleaned_headers, "Referer: http://212.129.223.4:8080/\r\n");
    strcat(cleaned_headers, "Connection: close\r\n");

    char modified_request[8192];
    int new_len = sprintf(modified_request, "%s%s\r\n", request_line, cleaned_headers);
    memcpy(modified_request + new_len, body, body_len);
    new_len += body_len;

    printf("转发内容如下：\n%.*s\n", new_len, modified_request);

    if (send(backend_sock, modified_request, new_len, 0) < 0)
    {
        send(client_sock, "HTTP/1.1 500 Internal Server Error\r\n\r\n", 38, 0);
        close(backend_sock);
        return;
    }

    // 接收后端服务器的响应
    int first_packet = 1;
    char response_buffer[8192];
    int bytes;
    while ((bytes = recv(backend_sock, response_buffer, sizeof(response_buffer), 0)) > 0)
    {
        if (first_packet)
        {
            char *header_end = strstr(response_buffer, "\r\n\r\n");
            if (header_end)
            {
                int head_len = header_end - response_buffer;
                char original_headers[8192] = {0};
                strncpy(original_headers, response_buffer, head_len);

                // 清理已有 CORS 字段
                char *line = strtok(original_headers, "\r\n");
                char final_headers[8192] = {0};
                while (line != NULL)
                {
                    if (!strstr(line, "Access-Control-Allow-Origin") && !strstr(line, "Access-Control-Allow-Credentials"))
                    {
                        strcat(final_headers, line);
                        strcat(final_headers, "\r\n");
                    }
                    line = strtok(NULL, "\r\n");
                }

                // 添加自定义跨域响应字段，添加响应体并发回前端
                char new_response[9000] = {0};
                int pos = 0;
                pos += sprintf(new_response + pos, "%s", final_headers);
                pos += sprintf(new_response + pos,
                               "Access-Control-Allow-Origin: http://212.129.223.4:8080\r\n"
                               "Access-Control-Allow-Credentials: true\r\n\r\n");
                strcpy(new_response + pos, header_end + 4);

                int total_len = strlen(new_response);
                send(client_sock, new_response, total_len, 0);
            }
            else // 原样中转
            {
                send(client_sock, response_buffer, bytes, 0);
            }
            first_packet = 0;
        }
        else
        {
            send(client_sock, response_buffer, bytes, 0);
        }
    }

    close(backend_sock);
}
