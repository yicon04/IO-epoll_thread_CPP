#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <chrono>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9999

// ===== 配置参数 =====
const int THREAD_NUM = 100;      // 并发线程数（=连接数）
const int REQ_PER_THREAD = 1000; // 每个线程请求数

std::atomic<int> success_cnt(0);

// 每个线程执行
void worker(int id)
{
    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd == -1)
    {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (connect(cfd, (sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect");
        close(cfd);
        return;
    }
    std::cout << "连接成功，线程ID: " << std::this_thread::get_id() << std::endl;

    char sendbuf[256];
    char recvbuf[1024];

    for (int i = 0; i < REQ_PER_THREAD; i++)
    {
        snprintf(sendbuf, sizeof(sendbuf), "msg-%d-%d\n", id, i);

        // 发送
        if (send(cfd, sendbuf, strlen(sendbuf), 0) <= 0)
        {
            break;
        }

        // 接收
        int len = recv(cfd, recvbuf, sizeof(recvbuf) - 1, 0);
        if (len <= 0)
        {
            break;
        }

        recvbuf[len] = '\0';

        // 简单校验（可选）
        success_cnt++;
    }

    close(cfd);
}

int main()
{
    std::vector<std::thread> threads;

    auto start = std::chrono::steady_clock::now();

    // 启动线程
    for (int i = 0; i < THREAD_NUM; i++)
    {
        threads.emplace_back(worker, i);
    }

    // 等待结束
    for (auto &t : threads)
    {
        t.join();
    }

    auto end = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();

    int total_req = THREAD_NUM * REQ_PER_THREAD;

    std::cout << "=========================\n";
    std::cout << "总请求数: " << total_req << std::endl;
    std::cout << "成功请求: " << success_cnt.load() << std::endl;
    std::cout << "总耗时: " << seconds << " 秒\n";
    std::cout << "QPS: " << (int)(success_cnt / seconds) << std::endl;
    std::cout << "=========================\n";

    return 0;
}