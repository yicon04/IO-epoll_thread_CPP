#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

int main()
{
    int cfd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in saddr{};
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr);

    connect(cfd, (sockaddr *)&saddr, sizeof(saddr));

    char buf[1024];

    for (int i = 0; i < 100; i++)
    {
        snprintf(buf, sizeof(buf), "hello,world,%d\n", i);

        send(cfd, buf, strlen(buf), 0);
        std::cout << "Send: " << buf;

        int len = recv(cfd, buf, sizeof(buf) - 1, 0);
        if (len <= 0)
            break;

        buf[len] = '\0';
        std::cout << "Recv: " << buf << std::endl;

        usleep(100000);
    }

    close(cfd);
}