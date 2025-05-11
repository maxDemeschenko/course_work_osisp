#pragma once

#include <sys/inotify.h>
#include <unistd.h>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <functional>
#include <iomanip>

class InotifyWatcher {
public:
    InotifyWatcher() : running(false) {
        inotifyFd = inotify_init1(IN_NONBLOCK);
        if (inotifyFd < 0) {
            throw std::runtime_error("Не удалось инициализировать inotify");
        }
    }

    ~InotifyWatcher() {
        stop();
        close(inotifyFd);
    }

    void addWatch(const std::string& path, std::function<void(uint32_t)> callback) {
        int wd = inotify_add_watch(inotifyFd, path.c_str(),
                                   IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE_SELF | IN_ATTRIB);
        if (wd < 0) {
            throw std::runtime_error("Не удалось добавить inotify watch на: " + path);
        }
        watchMap[wd] = callback;
    }

    void start() {
        running = true;
        watchThread = std::thread([this]() {
            char buffer[4096]
                __attribute__((aligned(__alignof__(struct inotify_event))));
            while (running) {
                ssize_t length = read(inotifyFd, buffer, sizeof(buffer));
                if (length < 0) {
                    if (errno == EAGAIN) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }
                    perror("read");
                    break;
                }

                for (char* ptr = buffer; ptr < buffer + length;) {
                    struct inotify_event* event = (struct inotify_event*)ptr;
                    auto it = watchMap.find(event->wd);
                    if (it != watchMap.end()) {
                        it->second(event->mask);  // вызываем callback
                    }
                    ptr += sizeof(struct inotify_event) + event->len;
                }
            }
        });
    }

    void stop() {
        running = false;
        if (watchThread.joinable()) {
            watchThread.join();
        }
    }

private:
    int inotifyFd;
    std::atomic<bool> running;
    std::unordered_map<int, std::function<void(uint32_t)>> watchMap;
    std::thread watchThread;
};
