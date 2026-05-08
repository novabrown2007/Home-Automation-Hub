#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class ThreadManager {
public:
    using Task = std::function<void()>;
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::milliseconds;
    using JobId = std::uint64_t;

    struct BackgroundJob {
        std::string name;
        Duration interval{1000};
        Task task;
        bool runImmediately{true};
    };

    ThreadManager();
    ~ThreadManager();

    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;

    void start(std::size_t eventWorkerCount = 2, std::size_t userWorkerCount = 1);
    void stop();

    [[nodiscard]] bool isRunning() const;

    JobId registerBackgroundJob(BackgroundJob job);
    bool removeBackgroundJob(JobId jobId);

    std::future<void> submitEventTask(std::string name, Task task);
    std::future<void> submitUserTask(std::string name, Task task);

    [[nodiscard]] std::size_t backgroundJobCount() const;
    [[nodiscard]] std::size_t pendingEventTaskCount() const;
    [[nodiscard]] std::size_t pendingUserTaskCount() const;

private:
    struct BackgroundJobSlot {
        BackgroundJob job;
        std::shared_ptr<std::atomic<bool>> enabled;
    };

    struct QueuedTask {
        std::string name;
        Task task;
        std::promise<void> completion;
    };

    void startBackgroundWorkers();
    void startQueueWorkers(
        std::size_t workerCount,
        std::vector<std::jthread>& workers,
        std::queue<QueuedTask>& queue,
        std::mutex& queueMutex,
        std::condition_variable& queueCondition,
        const char* workerLabel
    );

    void runBackgroundJob(
        const BackgroundJob& job,
        const std::shared_ptr<std::atomic<bool>>& enabled,
        std::stop_token stopToken
    ) const;

    static std::future<void> makeRejectedFuture(const std::string& reason);

    mutable std::mutex lifecycleMutex;
    std::atomic<bool> running;

    std::atomic<JobId> nextJobId;
    mutable std::mutex backgroundMutex;
    std::unordered_map<JobId, BackgroundJobSlot> backgroundJobs;
    std::vector<std::jthread> backgroundWorkers;

    mutable std::mutex eventMutex;
    std::condition_variable eventCondition;
    std::queue<QueuedTask> eventQueue;
    std::vector<std::jthread> eventWorkers;

    mutable std::mutex userMutex;
    std::condition_variable userCondition;
    std::queue<QueuedTask> userQueue;
    std::vector<std::jthread> userWorkers;
};
