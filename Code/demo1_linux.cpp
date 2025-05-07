#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define PORT 9090
#define BUFFER_SIZE 1024

void send_html(int s, const char *filename);
void *handle_client(void *client_socket);
void send_error(int s, const char *error_message);
void parse_request(char *request, char *method, char *path);
void handle_post(int client, const char *body);
void forward_to_backend(int client_sock, const char *original_request);

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

    int server = socket(AF_INET, SOCK_STREAM, 0);
    merror(server, -1, "create socket fail");

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int isok = bind(server, (struct sockaddr *)&server_addr, sizeof(server_addr));
    merror(isok, -1, "bind fail");

    isok = listen(server, 5);
    merror(isok, -1, "listen fail");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    while (1)
    {
        int client = accept(server, (struct sockaddr *)&client_addr, &client_len);
        merror(client, -1, "accept fail");

        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        *pclient = client;
        pthread_create(&tid, NULL, handle_client, pclient);
        pthread_detach(tid);
    }

    close(server);
    return 0;
}

void parse_request(char *request, char *method, char *path)
{
    sscanf(request, "%s %s", method, path);
}

void *handle_client(void *client_socket)
{
    int client = *(int *)client_socket;
    free(client_socket);

    char buffer[BUFFER_SIZE] = "";
    int bytes_received = recv(client, buffer, sizeof(buffer), 0);

    if (bytes_received <= 0)
    {
        close(client);
        return NULL;
    }

    printf("Request: %s\n", buffer);

    // char method[10], path[1024];
    // parse_request(buffer, method, path);

    // if (strcmp(method, "GET") == 0)
    // {
    //     if (strcmp(path, "/demo") == 0)
    //     {
    //         send_html(client, "index.html");
    //     }
    //     else
    //     {
    //         send_error(client, "404 Not Found");
    //     }
    // }
    // else if (strcmp(method, "POST") == 0)
    // {
    //     char *body = strstr(buffer, "\r\n\r\n");
    //     if (body != NULL)
    //     {
    //         body += 4;
    //         handle_post(client, body);
    //     }
    //     else
    //     {
    //         send_error(client, "400 Bad Request");
    //     }
    // }
    // else
    // {
    //     send_error(client, "405 Method Not Allowed");
    // }
    forward_to_backend(client, buffer);
    close(client);
    return NULL;
}

void handle_post(int client, const char *body)
{
    printf("POST data received: %s\n", body);
    const char *response = "HTTP/1.1 200 OK\r\nContent-Type:text/html\r\n\r\n<h1>POST data received successfully!</h1>";
    send(client, response, strlen(response), 0);
}

void send_html(int s, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        send_error(s, "404 Not Found");
        return;
    }

    char response_header[512];
    sprintf(response_header, "HTTP/1.1 200 OK\r\nContent-Type:text/html\r\n\r\n");
    send(s, response_header, strlen(response_header), 0);

    char temp[1024] = "";
    while (fgets(temp, sizeof(temp), fp))
    {
        send(s, temp, strlen(temp), 0);
    }
    fclose(fp);
}

void send_error(int s, const char *error_message)
{
    char response_header[512];
    sprintf(response_header, "HTTP/1.1 %s\r\nContent-Type:text/html\r\n\r\n", error_message);
    send(s, response_header, strlen(response_header), 0);

    char error_page[1024];
    sprintf(error_page, "<html><body><h1>Error: %s</h1></body></html>", error_message);
    send(s, error_page, strlen(error_page), 0);
}

void forward_to_backend(int client_sock, const char *original_request)
{
    // 1. 创建连接到 Java Web 的 socket（127.0.0.1:8080）
    int backend_sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in backend_addr;
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(8080); // Java 项目的端口
    backend_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(backend_sock, (struct sockaddr *)&backend_addr, sizeof(backend_addr)) < 0)
    {
        perror("连接后端失败");
        close(backend_sock);
        return;
    }

    // 2. 把浏览器的请求转发给后端 Java 服务
    send(backend_sock, original_request, strlen(original_request), 0);

    // 3. 接收 Java 后端响应
    char buffer[4096];
    int bytes;

    while ((bytes = recv(backend_sock, buffer, sizeof(buffer), 0)) > 0)
    {
        // 4. 将响应转发给浏览器
        send(client_sock, buffer, bytes, 0);
    }

    close(backend_sock);
}
