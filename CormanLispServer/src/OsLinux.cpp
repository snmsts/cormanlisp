//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		OsLinux.cpp
//		Contents:	What the kernel asks of the operating system, on Linux.
//

#include "Stdafx.h"
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <malloc.h>
#include <pwd.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include "Os.h"

//
//	Memory
//
//	The kernel keeps addresses in signed words, so its memory has to lie
//	in the lower half of the address space, where the system does not put
//	it unasked.
//
static const unsigned long LowestAddress = 0x10000000;
static const unsigned long AddressLimit = 0x80000000;
static const unsigned long AddressStep = 0x01000000;

static void* mapLow(unsigned long size, int protection)
{
	for (unsigned long at = LowestAddress; at < AddressLimit && AddressLimit - at >= size; at += AddressStep)
	{
		void* p = mmap((void*)at, size, protection,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
		if (p == MAP_FAILED)
			continue;
		if (p == (void*)at)
			return p;
		munmap(p, size);		// a system which took the address for a hint
	}
	return 0;
}

//	What osAllocateMemory() has handed out, to know how much to give back.
struct Allocation
{
	Allocation* next;
	void* address;
	unsigned long size;
};
static Allocation* Allocations = 0;
static pthread_mutex_t AllocationsLock = PTHREAD_MUTEX_INITIALIZER;

void* osReserveMemory(unsigned long size)
{
	return mapLow(size, PROT_NONE);
}

bool osCommitMemory(void* address, unsigned long size)
{
	return mprotect(address, size, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
}

bool osDecommitMemory(void* address, unsigned long size)
{
	// mapping the range afresh drops what was in it
	return mmap(address, size, PROT_NONE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0) == address;
}

void* osAllocateMemory(unsigned long size)
{
	void* p = mapLow(size, PROT_READ | PROT_WRITE | PROT_EXEC);
	if (p)
	{
		Allocation* a = (Allocation*)malloc(sizeof(Allocation));
		a->address = p;
		a->size = size;
		pthread_mutex_lock(&AllocationsLock);
		a->next = Allocations;
		Allocations = a;
		pthread_mutex_unlock(&AllocationsLock);
	}
	return p;
}

void osFreeMemory(void* address)
{
	Allocation* found = 0;
	if (!address)
		return;
	pthread_mutex_lock(&AllocationsLock);
	for (Allocation** a = &Allocations; *a; a = &(*a)->next)
	{
		if ((*a)->address == address)
		{
			found = *a;
			*a = found->next;
			break;
		}
	}
	pthread_mutex_unlock(&AllocationsLock);
	if (found)
	{
		munmap(found->address, found->size);
		free(found);
	}
}

bool osCanRead(const void* address, unsigned long size)
{
	// The system reads the memory for us, and reports what this process
	// would have been signalled for.
	static int ends[2] = { -1, -1 };
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	const unsigned long pageSize = 0x1000;
	unsigned long first = (unsigned long)address & ~(pageSize - 1);
	unsigned long last = ((unsigned long)address + (size ? size - 1 : 0)) & ~(pageSize - 1);
	bool readable = true;

	pthread_mutex_lock(&lock);
	if (ends[0] < 0 && pipe(ends) != 0)
		readable = false;
	for (unsigned long page = first; readable; page += pageSize)
	{
		char byte = 0;
		if (write(ends[1], (void*)page, 1) != 1)
			readable = false;
		else if (read(ends[0], &byte, 1) != 1)
			readable = false;
		if (page == last)
			break;
	}
	pthread_mutex_unlock(&lock);
	return readable;
}

bool osGuardPage(void* address)
{
	return false;		// not on this system yet
}

void* osHeapAllocate(unsigned long size)
{
	return malloc(size);
}

void osHeapFree(void* memory)
{
	free(memory);
}

void* osHeapReallocate(void* memory, unsigned long size)
{
	size_t had = memory ? malloc_usable_size(memory) : 0;
	char* p = (char*)realloc(memory, size);
	if (p && size > had)
		memset(p + had, 0, size - had);
	return p;
}

bool osProtectMemory(void* address, unsigned long size, bool writable)
{
	return mprotect(address, size,
		writable ? PROT_READ | PROT_WRITE | PROT_EXEC : PROT_READ | PROT_EXEC) == 0;
}

bool osFlushInstructionCache()
{
	return true;		// nothing to do on this processor
}

//
//	Thread slots
//
OsThreadSlot osAllocateThreadSlot()
{
	pthread_key_t key = 0;
	pthread_key_create(&key, 0);
	return key;
}

void osFreeThreadSlot(OsThreadSlot slot)
{
	pthread_key_delete((pthread_key_t)slot);
}

void* osThreadSlotValue(OsThreadSlot slot)
{
	return pthread_getspecific((pthread_key_t)slot);
}

void osSetThreadSlotValue(OsThreadSlot slot, void* value)
{
	pthread_setspecific((pthread_key_t)slot, value);
}

//
//	Locks
//
void osInitializeLock(OsLock* lock)
{
	pthread_mutexattr_t attributes;
	pthread_mutexattr_init(&attributes);
	pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(lock, &attributes);
	pthread_mutexattr_destroy(&attributes);
}

void osDeleteLock(OsLock* lock)
{
	pthread_mutex_destroy(lock);
}

void __stdcall osEnterLock(OsLock* lock)
{
	pthread_mutex_lock(lock);
}

void __stdcall osLeaveLock(OsLock* lock)
{
	pthread_mutex_unlock(lock);
}

//
//	Events
//
struct EventState
{
	pthread_mutex_t lock;
	pthread_cond_t changed;
	bool set;
	bool resetsItself;
};

OsEvent osCreateEvent(bool resetsItself)
{
	EventState* e = (EventState*)malloc(sizeof(EventState));
	pthread_condattr_t attributes;
	pthread_condattr_init(&attributes);
	pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
	pthread_mutex_init(&e->lock, 0);
	pthread_cond_init(&e->changed, &attributes);
	pthread_condattr_destroy(&attributes);
	e->set = false;
	e->resetsItself = resetsItself;
	return e;
}

void osCloseEvent(OsEvent event)
{
	EventState* e = (EventState*)event;
	if (!e)
		return;
	pthread_cond_destroy(&e->changed);
	pthread_mutex_destroy(&e->lock);
	free(e);
}

void osSetEvent(OsEvent event)
{
	EventState* e = (EventState*)event;
	pthread_mutex_lock(&e->lock);
	e->set = true;
	if (e->resetsItself)
		pthread_cond_signal(&e->changed);
	else
		pthread_cond_broadcast(&e->changed);
	pthread_mutex_unlock(&e->lock);
}

void osResetEvent(OsEvent event)
{
	EventState* e = (EventState*)event;
	pthread_mutex_lock(&e->lock);
	e->set = false;
	pthread_mutex_unlock(&e->lock);
}

bool osWaitEvent(OsEvent event, unsigned long milliseconds)
{
	EventState* e = (EventState*)event;
	struct timespec until;
	bool set = false;

	if (milliseconds != OsForever)
	{
		clock_gettime(CLOCK_MONOTONIC, &until);
		until.tv_sec += milliseconds / 1000;
		until.tv_nsec += (milliseconds % 1000) * 1000000;
		if (until.tv_nsec >= 1000000000)
		{
			until.tv_sec++;
			until.tv_nsec -= 1000000000;
		}
	}
	pthread_mutex_lock(&e->lock);
	while (!e->set)
	{
		if (milliseconds == OsForever)
			pthread_cond_wait(&e->changed, &e->lock);
		else if (pthread_cond_timedwait(&e->changed, &e->lock, &until) != 0)
			break;
	}
	set = e->set;
	if (set && e->resetsItself)
		e->set = false;
	pthread_mutex_unlock(&e->lock);
	return set;
}

//
//	Threads
//
struct Thread
{
	pthread_t thread;
	unsigned long id;
	OsThreadProc proc;
	void* argument;
	OsEvent started;		// the thread has stored its id
	OsEvent resumed;		// the thread may call proc
	bool running;
	bool closed;
};

static pthread_mutex_t ThreadsLock = PTHREAD_MUTEX_INITIALIZER;
static __thread Thread* CurrentThread = 0;

//	A Thread is done with when it has ended and has been closed.
static void release(Thread* t, bool ended, bool closed)
{
	bool done = false;
	pthread_mutex_lock(&ThreadsLock);
	if (ended)
		t->running = false;
	if (closed)
		t->closed = true;
	done = !t->running && t->closed;
	pthread_mutex_unlock(&ThreadsLock);
	if (done)
	{
		osCloseEvent(t->started);
		osCloseEvent(t->resumed);
		free(t);
	}
}

static void* threadMain(void* argument)
{
	Thread* t = (Thread*)argument;
	CurrentThread = t;
	t->id = osCurrentThreadId();
	osSetEvent(t->started);
	osWaitEvent(t->resumed, OsForever);
	t->proc(t->argument);
	CurrentThread = 0;
	release(t, true, false);
	return 0;
}

OsThread osCreateThread(OsThreadProc proc, void* argument, unsigned long* id)
{
	Thread* t = (Thread*)malloc(sizeof(Thread));
	t->id = 0;
	t->proc = proc;
	t->argument = argument;
	t->started = osCreateEvent(false);
	t->resumed = osCreateEvent(false);
	t->running = true;
	t->closed = false;
	if (pthread_create(&t->thread, 0, threadMain, t) != 0)
	{
		t->running = false;
		release(t, true, true);
		return 0;
	}
	pthread_detach(t->thread);
	osWaitEvent(t->started, OsForever);
	*id = t->id;
	return t;
}

void osCloseThread(OsThread thread)
{
	if (thread)
		release((Thread*)thread, false, true);
}

void osExitThread(unsigned code)
{
	Thread* t = CurrentThread;
	CurrentThread = 0;
	if (t)
		release(t, true, false);
	pthread_exit(0);
}

bool osStartThreadServices()
{
	return true;
}

void osStopThreadServices()
{
}

unsigned long osCurrentThreadId()
{
	return (unsigned long)syscall(SYS_gettid);
}

OsThread osOpenCurrentThread()
{
	// a thread which the kernel did not create: nothing ends this record
	// for it, so closing it is what frees it
	Thread* t = (Thread*)malloc(sizeof(Thread));
	t->thread = pthread_self();
	t->id = osCurrentThreadId();
	t->proc = 0;
	t->argument = 0;
	t->started = 0;
	t->resumed = 0;
	t->running = false;
	t->closed = false;
	return t;
}

void* osStackTop()
{
	pthread_attr_t attributes;
	void* lowest = 0;
	size_t size = 0;
	if (pthread_getattr_np(pthread_self(), &attributes) != 0)
		return 0;
	pthread_attr_getstack(&attributes, &lowest, &size);
	pthread_attr_destroy(&attributes);
	return (char*)lowest + size;
}

bool osSuspendThread(OsThread thread)
{
	return false;		// not on this system yet
}

void osResumeThread(OsThread thread)
{
	// all that can be resumed so far is a thread which has not run yet
	Thread* t = (Thread*)thread;
	if (t && t->resumed)
		osSetEvent(t->resumed);
}

bool osGetThreadRegisters(OsThread thread, OsRegisters* registers)
{
	return false;		// not on this system yet
}

bool osSetThreadRegisters(OsThread thread, const OsRegisters* registers)
{
	return false;		// not on this system yet
}

//
//	Time
//
__int64 osPerformanceCounter()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (__int64)now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

__int64 osPerformanceFrequency()
{
	return 1000000;
}

unsigned long osMillisecondCount()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (unsigned long)((unsigned __int64)now.tv_sec * 1000 + now.tv_nsec / 1000000);
}

void osSleep(unsigned long milliseconds)
{
	struct timespec time;
	time.tv_sec = milliseconds / 1000;
	time.tv_nsec = (milliseconds % 1000) * 1000000;
	while (nanosleep(&time, &time) != 0 && errno == EINTR)
		;
}

static void fromTm(const struct tm* from, int milliseconds, OsTime* time)
{
	time->year = from->tm_year + 1900;
	time->month = from->tm_mon + 1;
	time->dayOfWeek = from->tm_wday;
	time->day = from->tm_mday;
	time->hour = from->tm_hour;
	time->minute = from->tm_min;
	time->second = from->tm_sec;
	time->milliseconds = milliseconds;
}

void osUniversalTime(OsTime* time)
{
	struct timespec now;
	struct tm parts;
	clock_gettime(CLOCK_REALTIME, &now);
	gmtime_r(&now.tv_sec, &parts);
	fromTm(&parts, now.tv_nsec / 1000000, time);
}

void osLocalTime(OsTime* time)
{
	struct timespec now;
	struct tm parts;
	clock_gettime(CLOCK_REALTIME, &now);
	localtime_r(&now.tv_sec, &parts);
	fromTm(&parts, now.tv_nsec / 1000000, time);
}

long osSecondsSince1970()
{
	return (long)time(0);
}

bool osTimeZone(long* minutesWest, bool* daylight)
{
	time_t now = time(0);
	struct tm parts;
	if (!localtime_r(&now, &parts))
		return false;
	*daylight = parts.tm_isdst > 0;

	// without what daylight saving time adds, as Windows counts it
	*minutesWest = -(parts.tm_gmtoff / 60) + (*daylight ? 60 : 0);
	return true;
}

//	The seconds from the start of 1601 to the start of 1970.
static const unsigned __int64 SecondsTo1970 = 11644473600ULL;

bool osTimeToFileTime(const OsTime* time, unsigned __int64* fileTime)
{
	struct tm parts;
	time_t seconds = 0;
	memset(&parts, 0, sizeof(parts));
	parts.tm_year = time->year - 1900;
	parts.tm_mon = time->month - 1;
	parts.tm_mday = time->day;
	parts.tm_hour = time->hour;
	parts.tm_min = time->minute;
	parts.tm_sec = time->second;
	seconds = timegm(&parts);
	if (seconds == (time_t)-1)
		return false;
	*fileTime = ((unsigned __int64)((__int64)seconds + (__int64)SecondsTo1970)) * 10000000
		+ (unsigned __int64)time->milliseconds * 10000;
	return true;
}

bool osFileTimeToTime(unsigned __int64 fileTime, OsTime* time)
{
	time_t seconds = (time_t)((__int64)(fileTime / 10000000) - (__int64)SecondsTo1970);
	struct tm parts;
	if (!gmtime_r(&seconds, &parts))
		return false;
	fromTm(&parts, (int)((fileTime / 10000) % 1000), time);
	return true;
}

//
//	Files
//
OsFile osOpenFile(const char* path, int access, int mode)
{
	int flags = (access & OsWriteAccess) ? ((access & OsReadAccess) ? O_RDWR : O_WRONLY) : O_RDONLY;
	struct stat status;
	int fd = -1;
	if (mode == OsCreateAlways)
		flags |= O_CREAT | O_TRUNC;
	else if (mode == OsOpenAlways)
		flags |= O_CREAT;
	fd = open(path, flags, 0666);
	if (fd < 0)
		return OsNoFile;
	if (fstat(fd, &status) == 0 && S_ISDIR(status.st_mode))
	{
		close(fd);
		errno = EISDIR;
		return OsNoFile;
	}
	return (OsFile)(long)fd;
}

bool osCloseFile(OsFile file)
{
	return close((int)(long)file) == 0;
}

bool osReadFile(OsFile file, void* buffer, unsigned long size, unsigned long* sizeRead)
{
	ssize_t n = read((int)(long)file, buffer, size);
	*sizeRead = n < 0 ? 0 : (unsigned long)n;
	return n >= 0;
}

bool osWriteFile(OsFile file, const void* buffer, unsigned long size, unsigned long* sizeWritten)
{
	unsigned long done = 0;
	while (done < size)
	{
		ssize_t n = write((int)(long)file, (const char*)buffer + done, size - done);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}
		done += (unsigned long)n;
	}
	*sizeWritten = done;
	return done == size;
}

unsigned long osFileSize(OsFile file)
{
	struct stat status;
	if (fstat((int)(long)file, &status) != 0)
		return (unsigned long)-1;
	return (unsigned long)status.st_size;
}

long osSeekFile(OsFile file, long offset, bool fromEnd)
{
	return (long)lseek((int)(long)file, offset, fromEnd ? SEEK_END : SEEK_SET);
}

bool osDeleteFile(const char* path)
{
	return unlink(path) == 0;
}

//	The seconds from the start of 1601 to the start of 1970.
static const unsigned __int64 FileTimeSecondsTo1970 = 11644473600ULL;

static unsigned __int64 fromTimespec(const struct timespec* time)
{
	return ((unsigned __int64)time->tv_sec + FileTimeSecondsTo1970) * 10000000
		+ (unsigned __int64)time->tv_nsec / 100;
}

static void toTimespec(unsigned __int64 from, struct timespec* time)
{
	time->tv_sec = (time_t)(from / 10000000 - FileTimeSecondsTo1970);
	time->tv_nsec = (long)(from % 10000000) * 100;
}

bool osFileTimes(OsFile file, unsigned __int64* created,
	unsigned __int64* accessed, unsigned __int64* written)
{
	struct stat status;
	if (fstat((int)(long)file, &status) != 0)
		return false;
	*created = fromTimespec(&status.st_ctim);
	*accessed = fromTimespec(&status.st_atim);
	*written = fromTimespec(&status.st_mtim);
	return true;
}

bool osSetFileTimes(OsFile file, const unsigned __int64* created,
	const unsigned __int64* accessed, const unsigned __int64* written)
{
	struct timespec times[2];
	toTimespec(*accessed, &times[0]);
	toTimespec(*written, &times[1]);
	return futimens((int)(long)file, times) == 0;
}

unsigned long osFileAttributes(const char* path)
{
	struct stat status;
	unsigned long attributes = 0;
	if (stat(path, &status) != 0)
		return (unsigned long)-1;
	if (S_ISDIR(status.st_mode))
		attributes |= OsFileDirectory;
	if (!(status.st_mode & S_IWUSR))
		attributes |= OsFileReadOnly;
	return attributes ? attributes : OsFileNormal;
}

bool osSetFileAttributes(const char* path, unsigned long attributes)
{
	struct stat status;
	mode_t mode = 0;
	if (stat(path, &status) != 0)
		return false;

	// all that can be set is whether the file can be written
	mode = status.st_mode & 07777;
	if (attributes & OsFileReadOnly)
		mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
	else
		mode |= S_IWUSR;
	return chmod(path, mode) == 0;
}

unsigned long osFullPath(const char* path, char* fullPath, unsigned long size)
{
	char from[4096];
	unsigned long length = 0;
	const char* p = from;

	// start from the current directory unless the path starts from the root
	if (path[0] == '/')
		from[0] = 0;
	else if (!getcwd(from, sizeof(from)))
		return 0;
	if (strlen(from) + 1 + strlen(path) + 1 > sizeof(from))
		return 0;
	strcat(from, "/");
	strcat(from, path);

	// copy the names, leaving out "." and backing up over ".."
	if (size < 2)
		return 0;
	while (*p)
	{
		const char* end = 0;
		unsigned long n = 0;
		while (*p == '/')
			p++;
		end = strchr(p, '/');
		n = end ? (unsigned long)(end - p) : strlen(p);
		if (n == 2 && p[0] == '.' && p[1] == '.')
		{
			while (length > 0 && fullPath[--length] != '/')
				;
		}
		else if (n > 0 && !(n == 1 && p[0] == '.'))
		{
			if (length + 1 + n + 1 > size)
				return 0;
			fullPath[length++] = '/';
			memcpy(fullPath + length, p, n);
			length += n;
		}
		p += n;
	}

	// a path which ends with a separator names a directory, and stays so
	if (length == 0 || (p > from && p[-1] == '/' && length + 2 <= size))
		fullPath[length++] = '/';
	fullPath[length] = 0;
	return length;
}

//
//	Directories
//
bool osCurrentDirectory(char* path, unsigned long size)
{
	return getcwd(path, size) != 0;
}

bool osSetCurrentDirectory(const char* path)
{
	return chdir(path) == 0;
}

bool osCreateDirectory(const char* path)
{
	return mkdir(path, 0777) == 0;
}

bool osMoveFile(const char* from, const char* to)
{
	return rename(from, to) == 0;
}

struct FileSearch
{
	DIR* directory;
	char path[4096];		// of the directory, ending with its separator
	char pattern[256];
};

OsFileSearch osFindFirstFile(const char* pattern, OsFoundFile* found)
{
	FileSearch* search = 0;
	const char* name = strrchr(pattern, '/');
	size_t pathLength = name ? (size_t)(name + 1 - pattern) : 0;
	name = name ? name + 1 : pattern;
	if (pathLength >= sizeof(search->path) || strlen(name) >= sizeof(search->pattern))
		return 0;

	search = (FileSearch*)malloc(sizeof(FileSearch));
	memcpy(search->path, pattern, pathLength);
	search->path[pathLength] = 0;

	// what stands for every name on Windows, where a name has a dot or is
	// taken to end with one
	strcpy(search->pattern, strcmp(name, "*.*") == 0 ? "*" : name);

	search->directory = opendir(pathLength ? search->path : ".");
	if (search->directory && osFindNextFile(search, found))
		return search;
	osFindClose(search);
	return 0;
}

bool osFindNextFile(OsFileSearch search, OsFoundFile* found)
{
	FileSearch* s = (FileSearch*)search;
	struct dirent* entry = 0;
	while ((entry = readdir(s->directory)) != 0)
	{
		char path[sizeof(s->path) + sizeof(entry->d_name)];
		struct stat status;
		if (fnmatch(s->pattern, entry->d_name, 0) != 0 || strlen(entry->d_name) >= sizeof(found->name))
			continue;
		strcpy(path, s->path);
		strcat(path, entry->d_name);
		if (stat(path, &status) != 0)
			memset(&status, 0, sizeof(status));
		found->isDirectory = S_ISDIR(status.st_mode) ? 1 : 0;
		found->size = (unsigned long)status.st_size;
		strcpy(found->name, entry->d_name);
		return true;
	}
	return false;
}

void osFindClose(OsFileSearch search)
{
	FileSearch* s = (FileSearch*)search;
	if (!s)
		return;
	if (s->directory)
		closedir(s->directory);
	free(s);
}

//
//	Libraries
//
void* osLoadLibrary(const char* path)
{
	return dlopen(path, RTLD_NOW | RTLD_GLOBAL);
}

bool osFreeLibrary(void* library)
{
	return dlclose(library) == 0;
}

void* osLibrarySymbol(void* library, const char* name)
{
	return dlsym(library, name);
}

unsigned long osLastError()
{
	return (unsigned long)errno;
}

const char* osName()
{
	return "LINUX";
}

//
//	The process
//
unsigned long osProcessId()
{
	return (unsigned long)getpid();
}

void* osProcessHandle()
{
	return (void*)(long)getpid();
}

void osExitProcess(unsigned code)
{
	exit((int)code);
}

unsigned long osModulePath(void* module, char* path, unsigned long size)
{
	ssize_t length = 0;
	if (module || size == 0)
		return 0;
	length = readlink("/proc/self/exe", path, size - 1);
	if (length < 0)
		length = 0;
	path[length] = 0;
	return (unsigned long)length;
}

void osMarkKernelLoaded()
{
}

//
//	The user
//
static bool userRecord(struct passwd* record, char* text, size_t size)
{
	struct passwd* found = 0;
	return getpwuid_r(getuid(), record, text, size, &found) == 0 && found;
}

bool osUserName(char* name, unsigned long size)
{
	struct passwd record;
	char text[1024];
	if (!userRecord(&record, text, sizeof(text)))
		return false;
	return strcpy_s(name, size, record.pw_name) == 0;
}

bool osUserDirectory(char* path, unsigned long size)
{
	const char* home = getenv("HOME");
	struct passwd record;
	char text[1024];
	if (!home || !*home)
	{
		if (!userRecord(&record, text, sizeof(text)))
			return false;
		home = record.pw_dir;
	}
	if (strcpy_s(path, size, home) != 0)
		return false;
	if (path[strlen(path) - 1] != '/' && strcat_s(path, size, "/") != 0)
		return false;
	return true;
}

bool osUserDocumentsDirectory(char* path, unsigned long size)
{
	return osUserDirectory(path, size) && strcat_s(path, size, "Documents/") == 0;
}

void osDebugMessage(const char* message)
{
	fputs(message, stderr);
}

void osShowMessage(void* window, const char* text, const char* title, bool inFront)
{
	fprintf(stderr, "%s: %s\n", title, text);
}
