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

#include <windows.h>
#include <shellapi.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>

#include "ErrorMessage.h"
#include "CormanLispHost.h"

static char gModuleName[MAX_PATH];
static char gImageName [MAX_PATH];
bool isTemplateApp = false;

const char SERVER_TITLE[] = "CormanLispServer.dll";
char LispServerPath[MAX_PATH + 1 + sizeof(SERVER_TITLE)];

static void outputConsoleText(char* text);
static char* getConsoleText();
static HINSTANCE getLocalCormanLispServer();
static bool loadKernel();
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
	return (ShellExecuteA(NULL, "open", file, NULL, NULL, SW_SHOWNORMAL) > (HINSTANCE)32 ? 0 : -1);
}

char* execFile = 0;

int mainx(int argc, char* argv[])
{
	if (!loadKernel())
		return FALSE;

	CormanLispHost host = { 0 };
	host.outputText = consoleOutputText;
	host.shutdown = consoleShutdown;
	host.openURL = consoleOpenURL;

	if (!KernelInitialize(&host, gImageName, CORMANLISP_CONSOLE_CLIENT))
	{
		fprintf(stderr, "The Lisp kernel could not be initialized");
		return FALSE;
	}
	kernelRunning = true;

	void* thread = 0;
	KernelRun(&thread);

	long numThreads = 0;
	char* input = 0;
	bool quitting = false;
	if (execFile)
	{
		LoadFile(execFile);
		char exitCommand[] = "(win:ExitProcess 0)\n";
		KernelProcessSource(exitCommand, sizeof(exitCommand));
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
			if (!_stricmp(input, ":quit\r\n"))
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
			char exitCommand[] = "(win:ExitProcess 0) \n #< expression not finished > \n (win:ExitProcess 0)\n";
			quitting = true;
			KernelProcessSource(exitCommand, sizeof(exitCommand));
			Sleep(1000);
		}
	}
	return 0;
}

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

long numThreads = 0;

BOOL __stdcall HandlerRoutine(DWORD dwCtrlType)
{
	BOOL handled = FALSE;
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
		if (kernelRunning)
		{
			numThreads = KernelNumThreads();
			if (numThreads > 0)
				KernelAbortThread();
		}
		handled = TRUE;
		break;
	case CTRL_BREAK_EVENT:
		if (kernelRunning)
		{
			numThreads = KernelNumThreads();
			if (numThreads > 0)
				KernelAbortThread();
		}
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

const char* consoleAppName = "CLCONSOLE.EXE";

int main(int argc, char* argv[])
{
	// use binary mode
	int result = _setmode(_fileno(stdout), _O_BINARY);
	result = _setmode(_fileno(stdin), _O_BINARY);

	BOOL ret = SetConsoleCtrlHandler(HandlerRoutine, TRUE);
	SetConsoleTitle("Corman Lisp Console");
#ifdef SET_CONSOLE_SCREEN_BUFFER_SIZE
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD coord;
	coord.X = 100;
	coord.Y = 100;		// default console size if 100 x 100
	SetConsoleScreenBufferSize(handle, coord);
#endif // SET_CONSOLE_SCREEN_BUFFER_SIZE
	GetModuleFileName(0, gModuleName, sizeof(gModuleName));

	isTemplateApp = _stricmp(argv[0], "clconsole") != 0;	// see if this app is being used
															// as a template

	char* imageName = 0;
	for (int i = 1; i < argc; i++)
	{
		if (!_stricmp(argv[i], "-image") && (i + 1) < argc)
		{
			imageName = argv[i+1];
		}
		if (!_stricmp(argv[i], "-execute") && (i + 1) < argc)
		{
			execFile = argv[i+1];
		}
	}
	if (imageName)
		strcpy_s(gImageName, sizeof(gImageName), imageName);
	else
			// default to the image has the same name as the executable,
			// but with a .img extension.
	{
		// as a special case, if this is the clconsole application 
		// default to CormanLisp.img
		unsigned int moduleNameLength = strlen(gModuleName);
		if (moduleNameLength >= strlen(consoleAppName)
			&& !_stricmp(gModuleName + (moduleNameLength - strlen(consoleAppName)), 
					consoleAppName))
        {
            // copy entire exe path and name
            strcpy_s(gImageName, sizeof(gImageName), gModuleName);
            // replace clconsole.exe with cormanlisp.img
			strcpy_s(gImageName + (moduleNameLength - strlen(consoleAppName)), sizeof(gImageName), "CormanLisp.img");
        }
		else
		{
			strcpy_s(gImageName, sizeof(gImageName), gModuleName);
		}
	}
	static char szAppName[] = "CormanLispConsole" ;
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

BYTE* MapFile(const char* path, DWORD* length)
{
	HANDLE hfile = CreateFile(path,
						GENERIC_READ,
						FILE_SHARE_READ,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
						NULL);
	if (hfile == INVALID_HANDLE_VALUE)
		return 0;
	*length = GetFileSize(hfile, 0);
	if (*length == 0xffffffff)
	{
		CloseHandle(hfile);
		return 0;
	}

	HANDLE hfilemap = CreateFileMapping(hfile,
						0,
						PAGE_READONLY,
						0, 0,
						0);

	CloseHandle(hfile);
	if (!hfilemap)
		return 0;

	BYTE* pbFile = (BYTE*)MapViewOfFile(hfilemap, FILE_MAP_READ, 0, 0, 0);
	CloseHandle(hfilemap);
	return pbFile;
}

void UnmapFile(BYTE* mapping)
{
	UnmapViewOfFile(mapping);
}

static void LoadFile(char* filename)
{
	char* ext = filename + strlen(filename);
	while (ext > filename && *ext != '.')
		ext--;
	if (*ext == '.' && !_stricmp(ext, ".fasl"))
	{
		// load a compiled file
		char command[512];
		strcpy_s(command, sizeof(command), "(common-lisp:load #P\"");
		strcat_s(command, sizeof(command), filename);
		strcat_s(command, sizeof(command), "\")");
		KernelProcessSource(command, strlen(command));
		return;
	}

	// load the file into memory
	DWORD fileSize;
	BYTE* data = MapFile(filename, &fileSize);

	if (!data)
	{
		fprintf(stderr, "Error: Could not open file %s.\n", filename);
		return;
	}

	// process code in the file
	KernelProcessSource((char*)data, fileSize);

	UnmapFile(data);
}
