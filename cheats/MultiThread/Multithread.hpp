#ifndef THREADING_H
#define THREADING_H

#include "mutex.h"
#include "semaphores.h"
#include "packed_heap.h"
#include "atomic_lock.h"
#include <atomic>
#include "shared_mutex.h"

#if defined(__posix__)
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef pthread_t thread_t;

#else
#define NOMINMAX
#include <Psapi.h>
#include "stdint.h"

typedef unsigned long thread_t;

#endif

typedef void(*JobFn)(void*);
typedef void*(__stdcall*threadFn)(void*);

struct Job
{
	JobFn function;
	void* args;
	bool ref;
	uint64_t id;

	Job()
	{
		function = nullptr;
		args = nullptr;
		ref = true;
		id = ~0ull;
	}
};

template <typename T>
struct LList
{

	struct LEntry
	{
		T entry;
		idx_t prev;
		idx_t next;
	};

	PackedHeapL<LEntry> entries;

	Mutex lock;
	bool quit;
	idx_t front;
	idx_t back;
	uint64_t lastID;
	uint64_t lastPopID;

	Semaphore sem;

	LList( ) {
		front = 0;
		back = 0;
		lastID = 0;
		lastPopID = 0;
		quit = false;
	}

	uint64_t Enqueue(const T& data, bool priority = false) {
		lock.lock();
		idx_t entry = entries.Alloc();
		if (priority) {
			entries[entry] = { data, front, 0 };
			entries[entry].entry.id = lastID;
			if (front)
				entries[front].next = entry;
			if (!back) {
				back = entry;
				entries[back].next = 0;
			}
			entries[entry].prev = front;
			front = entry;
		} else {
			entries[entry] = { data, 0, back };
			entries[entry].entry.id = lastID;
			if (back)
				entries[back].prev = entry;
			if (!front) {
				front = entry;
				entries[front].prev = 0;
			}
			entries[entry].next = back;
			back = entry;
		}
		uint64_t id = lastID++;
		lock.unlock();
		sem.Post();
		return id;
	}

	T DoPopFront(Mutex* lck) {
		lock.lock();
		if (!front) {
			lock.unlock();
			return Job();
		}
		if (lck)
			lck->lock();
		LEntry* entry = &entries[front];
		front = entry->prev;
		if (front)
			entries[front].next = 0;
		else
			back = 0;
		T ret = entry->entry;
		lastPopID = ret.id;
		entries.Free(entry);
		lock.unlock();
		return ret;
	}

	T PopFront(Mutex* lck = nullptr) {
		sem.Wait();
		if (quit) {
			sem.Post();
			return Job();
		}
		return DoPopFront(lck);
	}

	T TryPopFront() {
		if (sem.TimedWait(0))
			return Job();

		if (quit) {
			sem.Post();
			return Job();
		}

		return DoPopFront(nullptr);
	}

#ifdef _MSC_VER
	__declspec(noinline)
#else
	__attribute__((noinline))
#endif
	bool IsEmpty()
	{
		static volatile short cnt = 0;
		cnt++;
		return !front;
	}
};

struct JobThread
{
	std::atomic_bool shouldQuit;
	std::atomic_bool isRunning;
	std::atomic_bool queueEmpty;
	Mutex* jLock;
	LList<struct Job>* jobs;
	int id;
	void* handle;
};

namespace Threading
{
	inline unsigned int numThreads = 0;

	uint64_t _QueueJob(JobFn function, void* data, bool ref = false, bool priority = false);
	void InitThreads();
	int EndThreads();
	void FinishQueue(bool executeJobs = false);
	JobThread* BindThread(LList<struct Job>* jobsQueue);
	void UnbindThread(LList<struct Job>* jobsQueue);
	thread_t StartThread(threadFn start, void* param, bool detached = true);
	thread_t StartThread(threadFn start, void* param, bool detached, thread_t* thread);
	void JoinThread(thread_t thread, void** returnVal);

	template<typename N, typename T>
	uint64_t QueueJob(N function, T data, bool priority = false) {
		void* d = malloc(sizeof(T));
		memcpy(d, (void*)&data, sizeof(T)); //-V575
		return _QueueJob((JobFn)function, d, false, priority);
	}

	template<typename N, typename T>
	uint64_t QueueJobRef(N function, T* data, bool priority = false) {
		return _QueueJob((JobFn)function, (void*)data, true, priority);
	}
}

#endif
