#include "shared_mutex.h"

#if defined(__linux__) || defined(__APPLE__)
#include <pthread.h>

SharedMutex::SharedMutex() {
	lock = PTHREAD_RWLOCK_INITIALIZER;
}

SharedMutex::~SharedMutex() {
	pthread_rwlock_destroy(&lock);
}

void SharedMutex::rlock() {
	pthread_rwlock_rdlock(&lock);
}

bool SharedMutex::tryrlock() {
	int ret = pthread_rwlock_tryrdlock(&lock);
	return !ret;
}

void SharedMutex::runlock() {
	pthread_rwlock_unlock(&lock);
}

void SharedMutex::wlock() {
	pthread_rwlock_wrlock(&lock);
}

bool SharedMutex::trywlock() {
	int ret = pthread_rwlock_trywrlock(&lock);
	return !ret;
}

void SharedMutex::wunlock() {
	pthread_rwlock_unlock(&lock);
}

#else
#include <windows.h>

SharedMutex::SharedMutex() {
	::InitializeSRWLock(&lock);
}

SharedMutex::~SharedMutex() {
	//No release function
}

void SharedMutex::rlock() {
	::AcquireSRWLockShared(&lock);
}

bool SharedMutex::tryrlock() {
	return ::TryAcquireSRWLockShared(&lock);
}

void SharedMutex::runlock() {
	::ReleaseSRWLockShared(&lock);
}

void SharedMutex::wlock() {
	::AcquireSRWLockExclusive(&lock);
}

bool SharedMutex::trywlock() {
	return ::TryAcquireSRWLockExclusive(&lock);
}

void SharedMutex::wunlock() {
	::ReleaseSRWLockExclusive(&lock);
}
#endif
