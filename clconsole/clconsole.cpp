//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		clconsole.cpp
//		Contents:	Corman Lisp console application client.
//		Author:		Roger Corman
//		Created:	3/6/98
//

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "CormanLispHost.h"

//	What differs from one operating system to another is at the end.
static bool loadKernel();
static void prepareConsole();
static void getModulePath(char* path, unsigned long size);
static long openURL(const char* url);
static void sleepMilliseconds(unsigned long milliseconds);
static bool sameIgnoringCase(const char* a, const char* b);
extern const char* const consoleAppName;
extern const char* const exitCommand;
extern const char* const exitCommandWhenInputEnds;

static char gModuleName[260];
static char gImageName [260];
bool isTemplateApp = false;

static void outputConsoleText(char* text);
static char* getConsoleText();
static void LoadFile(char* filename);

//	The functions of the kernel, found in it once it has been loaded.
static long (*KernelInitialize)(const CormanLispHost* host, const char* imageName, int clientType) = 0;
static void (*KernelRun)(void** thread) = 0;
static void (*KernelProcessSource)(const char* text, long numChars) = 0;
static long (*KernelNumThreads)(void) = 0;
static void (*KernelAbortThread)(void) = 0;
static bool kernelRunning = false;

//	What the kernel asks of this program.
static void consoleOutputText(void* /*context*/, const char* text, long numBytes)
{
	for (int i = 0; i < numBytes; i++)
	{
		putchar(text[i]);
	}
	fflush(stdout);
}

static void consoleShutdown(void* /*context*/, const char* text, long numBytes)
{
	for (int i = 0; i < numBytes; i++)
	{
		putchar(text[i]);
	}
	fflush(stdout);
	exit(0);
}

static long consoleOpenURL(void* /*context*/, const char* file, void** /*window*/)
{
	return openURL(file);
}

//	Control-C and its like: Lisp stops what it is doing.
static void interruptLisp()
{
	if (kernelRunning && KernelNumThreads() > 0)
		KernelAbortThread();
}

char* execFile = 0;

int mainx(int argc, char* argv[])
{
	if (!loadKernel())
		return 0;

	CormanLispHost host = { 0 };
	host.outputText = consoleOutputText;
	host.shutdown = consoleShutdown;
	host.openURL = consoleOpenURL;

	if (!KernelInitialize(&host, gImageName, CORMANLISP_CONSOLE_CLIENT))
	{
		fprintf(stderr, "The Lisp kernel could not be initialized");
		return 0;
	}
	kernelRunning = true;

	void* thread = 0;
	KernelRun(&thread);

	char* input = 0;
	bool quitting = false;
	if (execFile)
	{
		LoadFile(execFile);
		KernelProcessSource(exitCommand, strlen(exitCommand) + 1);
	}
	/*
	else
	{
		if (!isTemplateApp)	// if the plain console app, put out this message
			outputConsoleText("Type :quit to exit.\r\n");
	}
	*/
	while (1)
	{
		if (!quitting)
			input = getConsoleText();
		if (strlen(input) > 0)
		{
			if (sameIgnoringCase(input, ":quit\r\n") || sameIgnoringCase(input, ":quit\n"))
				break;
			if (!quitting)
				KernelProcessSource(input, strlen(input));
		}
		else
		if (!quitting)
		{
			// construct this so normally the first expression will be
			// read and executed. However, if the reader was in the middle
			// of reading an expression, we don't want to hang with both
			// threads waiting for the other, so we follow if immediately
			// with an expression which will generate a read error (in most
			// situations--could still be defeated if it comes inside a string
			// or comment or something).
			quitting = true;
			KernelProcessSource(exitCommandWhenInputEnds, strlen(exitCommandWhenInputEnds) + 1);
			sleepMilliseconds(1000);
		}
	}
	return 0;
}

int main(int argc, char* argv[])
{
	prepareConsole();
	getModulePath(gModuleName, sizeof(gModuleName));

	isTemplateApp = !sameIgnoringCase(argv[0], "clconsole");	// see if this app is being used
															// as a template

	char* imageName = 0;
	for (int i = 1; i < argc; i++)
	{
		if (sameIgnoringCase(argv[i], "-image") && (i + 1) < argc)
		{
			imageName = argv[i+1];
		}
		if (sameIgnoringCase(argv[i], "-execute") && (i + 1) < argc)
		{
			execFile = argv[i+1];
		}
	}
	if (imageName)
		snprintf(gImageName, sizeof(gImageName), "%s", imageName);
	else
			// default to the image has the same name as the executable,
			// but with a .img extension.
	{
		// as a special case, if this is the clconsole application
		// default to CormanLisp.img
		unsigned int moduleNameLength = strlen(gModuleName);
		if (moduleNameLength >= strlen(consoleAppName)
			&& sameIgnoringCase(gModuleName + (moduleNameLength - strlen(consoleAppName)),
					consoleAppName))
        {
            // the path of the exe, with CormanLisp.img in place of its name
            snprintf(gImageName, sizeof(gImageName), "%.*s%s",
                (int)(moduleNameLength - strlen(consoleAppName)), gModuleName, "CormanLisp.img");
        }
		else
		{
			snprintf(gImageName, sizeof(gImageName), "%s", gModuleName);
		}
	}
	mainx(0, 0);
	return 0;
}

//	If an entire line has been entered, return it.
//	Otherwise return an empty string.
//
char ConsoleTextBuf[4096];
static char* s = ConsoleTextBuf;

static void outputConsoleText(char* text)
{
	char* p = text;
	while (*p)
	{
		putchar(*p++);
	}
}

static char* getConsoleText()
{
	while (1)
	{
		int c = getchar();

		if (c == EOF)
		{
		//	*s++ = 0;
		//	s = ConsoleTextBuf;
		//	return s;
		}
		else
		if (c == 10)
		{
			*s++ = (char)c;
			*s++ = 0;
			s = ConsoleTextBuf;
			return s;
		}
		else
			*s++ = (char)c;
	}
}

static void LoadFile(char* filename)
{
	char* ext = filename + strlen(filename);
	while (ext > filename && *ext != '.')
		ext--;
	if (*ext == '.' && sameIgnoringCase(ext, ".fasl"))
	{
		// load a compiled file
		char command[512];
		snprintf(command, sizeof(command), "(common-lisp:load #P\"%s\")", filename);
		KernelProcessSource(command, strlen(command));
		return;
	}

	// load the file into memory
	FILE* file = fopen(filename, "rb");
	long fileSize = 0;
	char* data = 0;
	if (file)
	{
		fseek(file, 0, SEEK_END);
		fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);
		if (fileSize >= 0)
			data = (char*)malloc(fileSize + 1);
		if (data && fread(data, 1, fileSize, file) != (size_t)fileSize)
		{
			free(data);
			data = 0;
		}
		fclose(file);
	}

	if (!data)
	{
		fprintf(stderr, "Error: Could not open file %s.\n", filename);
		return;
	}

	// process code in the file
	KernelProcessSource(data, fileSize);

	free(data);
}

#ifdef _WIN32

#include <windows.h>
#include <shellapi.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>

#include "ErrorMessage.h"

const char* const consoleAppName = "CLCONSOLE.EXE";
const char* const exitCommand = "(win:ExitProcess 0)\n";
const char* const exitCommandWhenInputEnds =
	"(win:ExitProcess 0) \n #< expression not finished > \n (win:ExitProcess 0)\n";

static const char SERVER_TITLE[] = "CormanLispServer.dll";
static char LispServerPath[MAX_PATH + 1 + sizeof(SERVER_TITLE)];

//
//	If a CormanLispServer.dll exists in the same directory as
//	the calling application, load it and return a handle to it.
//
static HINSTANCE getLocalCormanLispServer()
{
 	DWORD chars = GetModuleFileName(0, LispServerPath, sizeof(LispServerPath));
	int index = chars - 1;
	while (index >= 0 && LispServerPath[index] != '\\')
		index--;
	LispServerPath[index] = 0;	// get rid of file name, just leave the path
	if (chars > 0)
		strcat_s(LispServerPath, sizeof(LispServerPath), "\\");
	strcat_s(LispServerPath, sizeof(LispServerPath), SERVER_TITLE);

	return LoadLibrary(LispServerPath);
}

static bool loadKernel()
{
	HINSTANCE module = getLocalCormanLispServer();
	if (!module)
		module = LoadLibrary(SERVER_TITLE);
	if (!module)
	{
		ErrorMessage("Could not load CormanLispServer.dll");
		return false;
	}
	*(FARPROC*)&KernelInitialize = GetProcAddress(module, "CormanLispInitialize");
	*(FARPROC*)&KernelRun = GetProcAddress(module, "CormanLispRun");
	*(FARPROC*)&KernelProcessSource = GetProcAddress(module, "CormanLispProcessSource");
	*(FARPROC*)&KernelNumThreads = GetProcAddress(module, "CormanLispNumThreads");
	*(FARPROC*)&KernelAbortThread = GetProcAddress(module, "CormanLispAbortThread");
	if (!KernelInitialize || !KernelRun || !KernelProcessSource
			|| !KernelNumThreads || !KernelAbortThread)
	{
		ErrorMessage("Could not find the functions of the kernel in CormanLispServer.dll");
		return false;
	}
	return true;
}

static BOOL __stdcall HandlerRoutine(DWORD dwCtrlType)
{
	BOOL handled = FALSE;
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		interruptLisp();
		handled = TRUE;
		break;
	case CTRL_CLOSE_EVENT:
		break;
	case CTRL_LOGOFF_EVENT:
		break;
	case CTRL_SHUTDOWN_EVENT:
		break;
	}
	return handled;
}

static void prepareConsole()
{
	// use binary mode
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stdin), _O_BINARY);

	SetConsoleCtrlHandler(HandlerRoutine, TRUE);
	SetConsoleTitle("Corman Lisp Console");
#ifdef SET_CONSOLE_SCREEN_BUFFER_SIZE
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD coord;
	coord.X = 100;
	coord.Y = 100;		// default console size if 100 x 100
	SetConsoleScreenBufferSize(handle, coord);
#endif // SET_CONSOLE_SCREEN_BUFFER_SIZE
}

static void getModulePath(char* path, unsigned long size)
{
	GetModuleFileName(0, path, size);
}

static long openURL(const char* url)
{
	return (ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL) > (HINSTANCE)32 ? 0 : -1);
}

static void sleepMilliseconds(unsigned long milliseconds)
{
	Sleep(milliseconds);
}

static bool sameIgnoringCase(const char* a, const char* b)
{
	return _stricmp(a, b) == 0;
}

#else

#include <signal.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

const char* const consoleAppName = "cormanlisp";

//	The kernel tells this program to shut down, and it exits.
const char* const exitCommand = "(cl::%lisp-shutdown \"\")\n";
const char* const exitCommandWhenInputEnds =
	"(cl::%lisp-shutdown \"\") \n #< expression not finished > \n (cl::%lisp-shutdown \"\")\n";

//	The kernel is part of this program.
static bool loadKernel()
{
	KernelInitialize = CormanLispInitialize;
	KernelRun = CormanLispRun;
	KernelProcessSource = CormanLispProcessSource;
	KernelNumThreads = CormanLispNumThreads;
	KernelAbortThread = CormanLispAbortThread;
	return true;
}

static void interrupted(int /*signal*/)
{
	interruptLisp();
}

static void prepareConsole()
{
	signal(SIGINT, interrupted);
}

static void getModulePath(char* path, unsigned long size)
{
	ssize_t length = readlink("/proc/self/exe", path, size - 1);
	path[length < 0 ? 0 : length] = 0;
}

static long openURL(const char* /*url*/)
{
	return -1;		// nothing to open one with
}

static void sleepMilliseconds(unsigned long milliseconds)
{
	struct timespec time;
	time.tv_sec = milliseconds / 1000;
	time.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&time, 0);
}

static bool sameIgnoringCase(const char* a, const char* b)
{
	return strcasecmp(a, b) == 0;
}

#endif
