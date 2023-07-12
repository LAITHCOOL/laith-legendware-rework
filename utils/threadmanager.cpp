#include "threadmanager.hpp"
#include "includes.hpp"

//mxvement
static auto allocate_thread_id = reinterpret_cast<void(*) ()>(GetProcAddress(GetModuleHandleA("tier0.dll"), "AllocateThreadID"));
static auto free_thread_id = reinterpret_cast<void(*) ()>(GetProcAddress(GetModuleHandleA("tier0.dll"), "FreeThreadID"));

void CThreadManager::ThreadFn() {
	allocate_thread_id();
	while (true) {
		const auto object = g_ThreadManager.AssignObject();
		if (object) {
			object->run();
			object->handling = false;
		}
		else break;
	}
	free_thread_id();
}
