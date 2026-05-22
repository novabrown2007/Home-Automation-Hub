#include "threadmanager.h"

#include <exception>
#include <stdexcept>
#include <utility>

#include "../logging/logger.h"

ThreadManager::ThreadManager()
    : running(false), nextJobId(1) {}

ThreadManager::~ThreadManager() {
    stop();
}

void ThreadManager::start(std::size_t eventWorkerCount, std::size_t userWorkerCount) {
    std::scoped_lock lock(lifecycleMutex);

    if (running.load()) {
        Logger::instance().warning("ThreadManager", "Start requested while thread manager is already running.");
        return;
    }

    running.store(true);
    Logger::instance().info(
        "ThreadManager",
        "Starting thread manager eventWorkers=" + std::to_string(eventWorkerCount == 0 ? 1 : eventWorkerCount) +
        " userWorkers=" + std::to_string(userWorkerCount == 0 ? 1 : userWorkerCount)
    );
    startBackgroundWorkers();
    startQueueWorkers(
        eventWorkerCount == 0 ? 1 : eventWorkerCount,
        eventWorkers,
        eventQueue,
        eventMutex,
        eventCondition,
        "event"
    );
    startQueueWorkers(
        userWorkerCount == 0 ? 1 : userWorkerCount,
        userWorkers,
        userQueue,
        userMutex,
        userCondition,
        "user"
    );
}

void ThreadManager::stop() {
    std::scoped_lock lock(lifecycleMutex);

    if (!running.exchange(false)) {
        Logger::instance().warning("ThreadManager", "Stop requested while thread manager was not running.");
        return;
    }

    Logger::instance().info("ThreadManager", "Stopping thread manager.");
    eventCondition.notify_all();
    userCondition.notify_all();

    backgroundWorkers.clear();
    eventWorkers.clear();
    userWorkers.clear();

    {
        std::lock_guard eventLock(eventMutex);
        while (!eventQueue.empty()) {
            auto queuedTask = std::move(eventQueue.front());
            eventQueue.pop();
            queuedTask.completion.set_exception(
                std::make_exception_ptr(std::runtime_error("ThreadManager stopped before event task execution."))
            );
        }
    }

    {
        std::lock_guard userLock(userMutex);
        while (!userQueue.empty()) {
            auto queuedTask = std::move(userQueue.front());
            userQueue.pop();
            queuedTask.completion.set_exception(
                std::make_exception_ptr(std::runtime_error("ThreadManager stopped before user task execution."))
            );
        }
    }
}

bool ThreadManager::isRunning() const {
    return running.load();
}

ThreadManager::JobId ThreadManager::registerBackgroundJob(BackgroundJob job) {
    if (!job.task) {
        throw std::invalid_argument("Background job must define a task.");
    }

    if (job.interval.count() <= 0) {
        throw std::invalid_argument("Background job interval must be positive.");
    }

    const JobId jobId = nextJobId.fetch_add(1);
    auto enabled = std::make_shared<std::atomic<bool>>(true);
    const std::string jobName = job.name;

    {
        std::lock_guard lock(backgroundMutex);
        backgroundJobs.emplace(jobId, BackgroundJobSlot{
            .job = std::move(job),
            .enabled = enabled
        });
    }

    if (running.load()) {
        std::lock_guard lock(backgroundMutex);
        backgroundWorkers.emplace_back([this, jobId, enabled](std::stop_token stopToken) {
            BackgroundJob jobCopy;
            {
                std::lock_guard innerLock(backgroundMutex);
                auto jobIt = backgroundJobs.find(jobId);
                if (jobIt == backgroundJobs.end()) {
                    return;
                }
                jobCopy = jobIt->second.job;
            }
            runBackgroundJob(jobCopy, enabled, stopToken);
        });
    }

    Logger::instance().info(
        "ThreadManager",
        "Registered background job id=" + std::to_string(jobId) +
        " name=" + jobName +
        " intervalMs=" + std::to_string(job.interval.count())
    );
    return jobId;
}

bool ThreadManager::removeBackgroundJob(JobId jobId) {
    std::lock_guard lock(backgroundMutex);
    auto jobIt = backgroundJobs.find(jobId);
    if (jobIt == backgroundJobs.end()) {
        Logger::instance().warning("ThreadManager", "Attempted to remove unknown background job id=" + std::to_string(jobId));
        return false;
    }

    jobIt->second.enabled->store(false);
    Logger::instance().info("ThreadManager", "Removed background job id=" + std::to_string(jobId) + " name=" + jobIt->second.job.name);
    backgroundJobs.erase(jobIt);
    return true;
}

std::future<void> ThreadManager::submitEventTask(std::string name, Task task) {
    if (!task) {
        Logger::instance().warning("ThreadManager", "Rejected event task because callable was empty.");
        return makeRejectedFuture("Event task must define a callable.");
    }

    if (!running.load()) {
        Logger::instance().warning("ThreadManager", "Rejected event task because thread manager is not running.");
        return makeRejectedFuture("ThreadManager is not running.");
    }

    QueuedTask queuedTask{
        .name = std::move(name),
        .task = std::move(task),
        .completion = std::promise<void>()
    };
    auto future = queuedTask.completion.get_future();

    {
        std::lock_guard lock(eventMutex);
        eventQueue.push(std::move(queuedTask));
        Logger::instance().debug("ThreadManager", "Queued event task name=" + name + " pending=" + std::to_string(eventQueue.size()));
    }

    eventCondition.notify_one();
    return future;
}

std::future<void> ThreadManager::submitUserTask(std::string name, Task task) {
    if (!task) {
        Logger::instance().warning("ThreadManager", "Rejected user task because callable was empty.");
        return makeRejectedFuture("User task must define a callable.");
    }

    if (!running.load()) {
        Logger::instance().warning("ThreadManager", "Rejected user task because thread manager is not running.");
        return makeRejectedFuture("ThreadManager is not running.");
    }

    QueuedTask queuedTask{
        .name = std::move(name),
        .task = std::move(task),
        .completion = std::promise<void>()
    };
    auto future = queuedTask.completion.get_future();

    {
        std::lock_guard lock(userMutex);
        userQueue.push(std::move(queuedTask));
        Logger::instance().debug("ThreadManager", "Queued user task name=" + name + " pending=" + std::to_string(userQueue.size()));
    }

    userCondition.notify_one();
    return future;
}

std::size_t ThreadManager::backgroundJobCount() const {
    std::lock_guard lock(backgroundMutex);
    return backgroundJobs.size();
}

std::size_t ThreadManager::pendingEventTaskCount() const {
    std::lock_guard lock(eventMutex);
    return eventQueue.size();
}

std::size_t ThreadManager::pendingUserTaskCount() const {
    std::lock_guard lock(userMutex);
    return userQueue.size();
}

void ThreadManager::startBackgroundWorkers() {
    std::lock_guard lock(backgroundMutex);

    for (const auto& [jobId, slot] : backgroundJobs) {
        backgroundWorkers.emplace_back([this, job = slot.job, enabled = slot.enabled](std::stop_token stopToken) {
            runBackgroundJob(job, enabled, stopToken);
        });
    }
}

void ThreadManager::startQueueWorkers(
    std::size_t workerCount,
    std::vector<std::jthread>& workers,
    std::queue<QueuedTask>& queue,
    std::mutex& queueMutex,
    std::condition_variable& queueCondition,
    const char* workerLabel
) {
    for (std::size_t index = 0; index < workerCount; ++index) {
        Logger::instance().info(
            "ThreadManager",
            "Starting " + std::string(workerLabel) + " worker index=" + std::to_string(index)
        );
        workers.emplace_back([this, &queue, &queueMutex, &queueCondition, workerLabel, index](std::stop_token stopToken) {
            while (!stopToken.stop_requested()) {
                QueuedTask queuedTask;

                {
                    std::unique_lock lock(queueMutex);
                    queueCondition.wait(lock, [&]() {
                        return stopToken.stop_requested() || !running.load() || !queue.empty();
                    });

                    if ((stopToken.stop_requested() || !running.load()) && queue.empty()) {
                        return;
                    }

                    queuedTask = std::move(queue.front());
                    queue.pop();
                }

                try {
                    if (!queuedTask.name.empty()) {
                        Logger::instance().debug(
                            "ThreadManager",
                            "Running " + std::string(workerLabel) +
                            " task name=" + queuedTask.name +
                            " worker=" + std::to_string(index)
                        );
                    }
                    queuedTask.task();
                    queuedTask.completion.set_value();
                    Logger::instance().debug(
                        "ThreadManager",
                        "Completed " + std::string(workerLabel) +
                        " task name=" + queuedTask.name +
                        " worker=" + std::to_string(index)
                    );
                } catch (...) {
                    Logger::instance().error(
                        "ThreadManager",
                        "Task failed type=" + std::string(workerLabel) +
                        " name=" + queuedTask.name +
                        " worker=" + std::to_string(index)
                    );
                    queuedTask.completion.set_exception(std::current_exception());
                }
            }
        });
    }
}

void ThreadManager::runBackgroundJob(
    const BackgroundJob& job,
    const std::shared_ptr<std::atomic<bool>>& enabled,
    std::stop_token stopToken
) const {
    auto nextRun = Clock::now();

    if (!job.runImmediately) {
        nextRun += job.interval;
    }

    while (!stopToken.stop_requested() && running.load() && enabled->load()) {
        std::this_thread::sleep_until(nextRun);

        if (stopToken.stop_requested() || !running.load() || !enabled->load()) {
            return;
        }

        try {
            job.task();
        } catch (const std::exception& ex) {
            Logger::instance().error("ThreadManager", "Background job failed name=" + job.name + " error=" + ex.what());
        } catch (...) {
            Logger::instance().error("ThreadManager", "Background job failed name=" + job.name + " error=unknown");
        }

        nextRun = Clock::now() + job.interval;
    }
}

std::future<void> ThreadManager::makeRejectedFuture(const std::string& reason) {
    std::promise<void> promise;
    promise.set_exception(std::make_exception_ptr(std::runtime_error(reason)));
    return promise.get_future();
}
