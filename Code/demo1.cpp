#include <stdio.h>
#include <winsock2.h> // 包含网络编程的头文件，引入静态库
#pragma comment(lib, "ws2_32.lib")

void sendhtml(SOCKET s, const char *filename);
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
    printf("Welcome\n");
    WSAData wsdata;
    int isok = WSAStartup(MAKEWORD(2, 2), &wsdata); // 确定socket版本信息
    merror(isok, WSAEINVAL, "ask socket fail");

    /*
    AF_INET: IPv4;
    SOCK_STREAM: TCP协议
    */
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    merror(server, INVALID_SOCKET, "create socket fail");

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;         // 和创建server一样
    server_addr.sin_port = htons(80);         // 端口号，网络是大端存储，pc是小段存储
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

        char revdata[1024] = "";
        recv(client, revdata, 1024, 0); // 接收数据
        printf("%s receive %d bytes\n", revdata, strlen(revdata));

        // char sendata[1024] = "<h1>I'm test</h1>";
        // send(client, sendata, sizeof(sendata), 0); // 发送数据
        const char *filename = "ChatGPT.html";
        sendhtml(client, filename);
        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    getchar();
    return 0;
}

void sendhtml(SOCKET s, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        printf("open file fail\n");
        return;
    }
    char temp[1024] = "";
    do
    {
        fgets(temp, 1024, fp);
        send(s, temp, strlen(temp), 0); // 发送数据
    } while (!feof(fp));
}