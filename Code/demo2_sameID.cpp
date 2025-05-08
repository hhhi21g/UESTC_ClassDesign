#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <pthread.h>

#define PORT 9090

void *handle_client(void *client_socket);
void send_error(SOCKET s, const char *error_message);
void forward_to_backend(SOCKET client_sock, const char *original_request, int total_len);
void load_cookie();
void save_cookie(const char *cookie_value);

int merror(SOCKET redata, SOCKET error, const char *showinfo)
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
    WSAData wsdata;
    int isok = WSAStartup(MAKEWORD(2, 2), &wsdata);
    merror(isok, WSAEINVAL, "ask socket fail");

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    merror(server, INVALID_SOCKET, "create socket fail");

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    isok = bind(server, (struct sockaddr *)&server_addr, sizeof(server_addr));
    merror(isok, SOCKET_ERROR, "bind fail");

    isok = listen(server, 5);
    merror(isok, SOCKET_ERROR, "listen fail");

    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    while (1)
    {
        SOCKET client = accept(server, (struct sockaddr *)&client_addr, &client_len);
        merror(client, INVALID_SOCKET, "accept fail");

        pthread_t tid;
        SOCKET *pclient = (SOCKET *)malloc(sizeof(SOCKET));
        *pclient = client;
        pthread_create(&tid, NULL, handle_client, (void *)pclient);
    }

    closesocket(server);
    WSACleanup();
    getchar();
    return 0;
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

    forward_to_backend(client, buffer, total);

    closesocket(client);
    return NULL;
}

void send_error(SOCKET s, const char *error_message)
{
    char response[1024];
    sprintf(response,
            "HTTP/1.1 %s\r\nContent-Type:text/plain\r\n\r\nError: %s",
            error_message, error_message);
    send(s, response, strlen(response), 0);
}

char saved_cookie[256] = "";

// 从文件加载 cookie
void load_cookie()
{
    FILE *fp = fopen("cookie.txt", "r");
    if (fp)
    {
        fgets(saved_cookie, sizeof(saved_cookie), fp);
        fclose(fp);
        // printf("📂 已加载 Cookie: %s\n", saved_cookie);
    }
}

// 保存 cookie 到文件
void save_cookie(const char *cookie_value)
{
    strncpy(saved_cookie, cookie_value, sizeof(saved_cookie) - 1);
    saved_cookie[sizeof(saved_cookie) - 1] = '\0';

    FILE *fp = fopen("cookie.txt", "w");
    if (fp)
    {
        fprintf(fp, "%s", saved_cookie);
        fclose(fp);
        // printf("💾 已保存 Cookie 到文件: %s\n", saved_cookie);
    }
}

void forward_to_backend(SOCKET client_sock, const char *original_request, int total_len)
{
    SOCKET backend_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in backend_addr;
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(8080);
    backend_addr.sin_addr.s_addr = inet_addr("212.129.223.4");

    if (connect(backend_sock, (struct sockaddr *)&backend_addr, sizeof(backend_addr)) < 0)
    {
        perror("❌ 连接 Java 后端失败");
        closesocket(backend_sock);
        return;
    }

    // 解析请求行
    char method[8], path[1024];
    sscanf(original_request, "%s %s", method, path);

    const char *first_line_end = strstr(original_request, "\r\n");
    if (!first_line_end)
    {
        send(client_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, 0);
        closesocket(backend_sock);
        return;
    }

    // 构建新的请求路径（加上 /webTest）
    char request_line[256];
    if (strcmp(path, "/upload") == 0 || strcmp(path, "/delete") == 0 || strcmp(path, "/memos") == 0)
        sprintf(request_line, "%s /webTest%s HTTP/1.1\r\n", method, path);
    else
        sprintf(request_line, "%s %s HTTP/1.1\r\n", method, path);

    // 分离 headers 与 body
    const char *header_start = first_line_end + 2;
    const char *header_end = strstr(header_start, "\r\n\r\n");
    if (!header_end)
    {
        send_error(client_sock, "400 Bad Request");
        closesocket(backend_sock);
        return;
    }

    int header_len = header_end - header_start;
    int body_len = total_len - (header_end + 4 - original_request);

    char headers[4096] = "", body[4096] = "";
    strncpy(headers, header_start, header_len);
    headers[header_len] = '\0';
    memcpy(body, header_end + 4, body_len);

    // 处理 headers（去除 Host / Content-Length / Cookie / Accept-Encoding）
    char cleaned_headers[4096] = "";
    char *line = strtok(headers, "\r\n");
    while (line)
    {
        if (strstr(line, "Host:") || strstr(line, "Content-Length:") ||
            strstr(line, "Cookie:") || strstr(line, "Accept-Encoding:"))
        {
            // 跳过这些字段
        }
        else
        {
            strcat(cleaned_headers, line);
            strcat(cleaned_headers, "\r\n");
        }
        line = strtok(NULL, "\r\n");
    }

    // 添加必要头部
    strcat(cleaned_headers, "Host: 212.129.223.4:8080\r\n");

    char clen[64];
    sprintf(clen, "Content-Length: %d\r\n", body_len);
    strcat(cleaned_headers, clen);
    // ✅ 必加（彻底禁用 chunked 和压缩）
    strcat(cleaned_headers, "Accept-Encoding: identity\r\n");
    // 禁用 chunked 编码，强制标准长度响应
    strcat(cleaned_headers, "Connection: close\r\n");
    strcat(cleaned_headers, "TE: trailers\r\n");
    strcat(cleaned_headers, "Origin: http://212.129.223.4:8080\r\n");
    strcat(cleaned_headers, "Referer: http://212.129.223.4:8080/\r\n");
    strcat(cleaned_headers, "Connection: close\r\n");

    // 加入 Cookie
    extern char saved_cookie[256];
    if (strlen(saved_cookie) > 0)
    {
        char cookie_line[512];
        sprintf(cookie_line, "Cookie: %s\r\n", saved_cookie);
        strcat(cleaned_headers, cookie_line);
        printf("🍪 插入 Cookie: %s", cookie_line);
    }

    // 构造最终请求
    char modified_request[8192];
    int new_len = sprintf(modified_request, "%s%s\r\n", request_line, cleaned_headers);
    memcpy(modified_request + new_len, body, body_len);
    new_len += body_len;

    printf("📤 转发内容如下：\n%.*s\n", new_len, modified_request);
    send(backend_sock, modified_request, new_len, 0);

    // 转发响应并提取 Cookie
    char response_buffer[8192];
    int bytes;
    while ((bytes = recv(backend_sock, response_buffer, sizeof(response_buffer), 0)) > 0)
    {
        fwrite(response_buffer, 1, bytes > 1024 ? 1024 : bytes, stdout);
        char *set_cookie = strstr(response_buffer, "Set-Cookie:");
        if (set_cookie)
        {
            char *jsid_start = strstr(set_cookie, "JSESSIONID=");
            if (jsid_start)
            {
                char *jsid_end = strchr(jsid_start, ';');
                if (jsid_end)
                    *jsid_end = '\0';
                // strncpy(saved_cookie, jsid_start, sizeof(saved_cookie) - 1);
                // saved_cookie[sizeof(saved_cookie) - 1] = '\0';
                // printf("✅ 已保存 Cookie: %s\n", saved_cookie);
                save_cookie(jsid_start);
            }
        }

        // send(client_sock, response_buffer, bytes, 0);
        // ✅ 原样转发整个响应
        int sent = 0;
        while (sent < bytes)
        {
            int s = send(client_sock, response_buffer + sent, bytes - sent, 0);
            if (s <= 0)
                break;
            sent += s;
        }
    }

    closesocket(backend_sock);
}
