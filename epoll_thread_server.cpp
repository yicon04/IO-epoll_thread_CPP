#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_EVENTS 1024

// ================== 工具函数 ==================
int setNonBlocking(int fd)
{
    int flag = fcntl(fd, F_GETFL);
    return fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

// ================== 任务结构 ==================
struct Task
{
    int fd;
    std::string data;
};

// ================== 线程池 ==================
class ThreadPool
{
public:
    ThreadPool(int num = 4) : stop(false)
    {
        for (int i = 0; i < num; i++)
        {
            workers.emplace_back([this]()
                                 {
                while (true)
                {
                    Task task;

                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [this]() { return stop || !tasks.empty(); });

                        if (stop && tasks.empty())
                            return;

                        task = tasks.front();
                        tasks.pop();
                    }

                    // ===== 业务处理 =====
                    for (auto &c : task.data)
                        c = toupper(c);

                    // ===== 回写 =====
                    std::cout << "after buf = " << task.data.c_str() << std::endl;
                    send(task.fd, task.data.c_str(), task.data.size(), 0);
                } });
        }
    }

    void addTask(Task t)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            tasks.push(t);
        }
        cv.notify_one();
    }

    ~ThreadPool()
    {
        stop = true;
        cv.notify_all();
        for (auto &t : workers)
            t.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<Task> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop;
};

// ================== 主程序 ==================
int main()
{
    int lfd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9999);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(lfd, (sockaddr *)&addr, sizeof(addr));
    listen(lfd, 128);

    setNonBlocking(lfd);

    int epfd = epoll_create(1);

    epoll_event ev{}, evs[MAX_EVENTS];
    ev.data.fd = lfd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);

    ThreadPool pool(4);

    std::cout << "服务器启动（epoll + 线程池）..." << std::endl;

    while (1)
    {
        int n = epoll_wait(epfd, evs, MAX_EVENTS, -1);

        for (int i = 0; i < n; i++)
        {
            int fd = evs[i].data.fd;

            // ===== 新连接 =====
            if (fd == lfd)
            {
                while (1)
                {
                    int cfd = accept(lfd, NULL, NULL);
                    if (cfd == -1)
                    {
                        if (errno == EAGAIN)
                        {
                            std::cout << "数据已经接收完毕" << std::endl;
                            break;
                        }
                        else
                        {
                            perror("accept");
                            epoll_ctl(lfd, EPOLL_CTL_DEL, cfd, NULL);
                            close(cfd);
                            break;
                        }
                    }

                    setNonBlocking(cfd);

                    ev.data.fd = cfd;
                    ev.events = EPOLLIN | EPOLLET;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);

                    std::cout << "新连接: " << cfd << std::endl;
                }
            }
            else
            {
                char buf[1024];
                std::string data;

                // ===== 读取数据（ET必须读干净）=====
                while (1)
                {
                    int len = recv(fd, buf, sizeof(buf), 0);
                    if (len > 0)
                    {
                        data.append(buf, len);
                    }

                    else if (len == 0)
                    {
                        std::cout << "客户端关闭: " << fd << std::endl;
                        close(fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        break;
                    }
                    else
                    {
                        if (errno == EAGAIN)
                        {
                            if (!data.empty())
                            {
                                // ===== 丢进线程池 =====
                                pool.addTask({fd, data});
                            }
                            break;
                        }
                        perror("recv");
                        break;
                    }
                }
            }
        }
    }

    close(lfd);
    close(epfd);
}