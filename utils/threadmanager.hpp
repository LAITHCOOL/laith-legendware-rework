#pragma once
#include <thread>
#include <vector>
#include <shared_mutex>
//mxvement

class BaseThreadObject {
public:
	bool handling = false;
	bool finished = false;
	virtual void run() = 0;
};

class CThreadManager {
public:
	std::vector<std::thread> thread_pool;
	CThreadManager() {
	}
	static void ThreadFn();
	mutable std::shared_mutex queue_mutex;                  // Prevents data races to the job queue
	[[nodiscard]] bool ObjectAvailable() const {
		std::unique_lock lock(queue_mutex);
		for (const auto object : objects) {
			if (!object->handling && !object->finished) {
				return true;
			}
		}
		return false;
	}
	bool Busy() {
		bool poolbusy;
		{
			poolbusy = ObjectAvailable();
		}
		return poolbusy;
	}
	void Start() {
		const uint32_t num_threads = std::thread::hardware_concurrency(); // Max # of threads the system supports
		thread_pool.resize(num_threads);
		for (auto& i : thread_pool) {
			i = std::thread(ThreadFn);
		}
	}
	void Wait() {
		for (auto& i : thread_pool) {
			if (i.joinable())
				i.join();
		}
		thread_pool.clear();
	}
	std::vector<BaseThreadObject*> objects = {};
	bool haulted = true;

	[[nodiscard]] BaseThreadObject* AssignObject() const {
		std::unique_lock lock(queue_mutex);
		for (const auto object : objects) {
			if (!object->handling && !object->finished) {
				object->handling = true;
				return object;
			}
		}

		return nullptr;
	}
};

inline CThreadManager g_ThreadManager;