#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <pthread.h>

#define PORT 9090

void *handle_client(void *client_socket);
void send_error(SOCKET s, const char *error_message);
void forward_to_backend(SOCKET client_sock, const char *original_request, int total_len);

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

void forward_to_backend(SOCKET client_sock, const char *original_request, int total_len)
{
    SOCKET backend_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in backend_addr;
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(8080);
    backend_addr.sin_addr.s_addr = inet_addr("212.129.223.4");

    if (connect(backend_sock, (struct sockaddr *)&backend_addr, sizeof(backend_addr)) < 0)
    {
        perror("连接 Java 后端失败");
        closesocket(backend_sock);
        return;
    }

    char method[8], path[1024];
    sscanf(original_request, "%s %s", method, path);

    const char *first_line_end = strstr(original_request, "\r\n");
    if (!first_line_end)
    {
        send_error(client_sock, "400 Bad Request");
        closesocket(backend_sock);
        return;
    }

    int remaining_len = total_len - (first_line_end + 2 - original_request);
    char request_line[256];

    if (strcmp(path, "/upload") == 0 || strcmp(path, "/delete") == 0)
        sprintf(request_line, "%s /webTest%s HTTP/1.1\r\n", method, path);
    else
        sprintf(request_line, "%s %s HTTP/1.1\r\n", method, path);

    char modified_request[8192];
    int new_len = sprintf(modified_request, "%s%.*s",
                          request_line,
                          remaining_len,
                          first_line_end + 2);

    send(backend_sock, modified_request, new_len, 0);

    printf("转发内容如下：\n%s\n", modified_request);

    char response_buffer[8192];
    int bytes;
    while ((bytes = recv(backend_sock, response_buffer, sizeof(response_buffer), 0)) > 0)
    {
        send(client_sock, response_buffer, bytes, 0);
    }

    closesocket(backend_sock);
}
