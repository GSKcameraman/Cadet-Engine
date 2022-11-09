#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>

static uint32_t s_mask = 0xffffffff;

static LONG debug_exception_handler(LPEXCEPTION_POINTERS info)
{
	// XXX: MS uses 0xE06D7363 to indicate C++ language exception.
	// We're just to going to ignore them. Sometimes Vulkan throws them on startup?
	// https://devblogs.microsoft.com/oldnewthing/20100730-00/?p=13273
	if (info->ExceptionRecord->ExceptionCode == 0xE06D7363)
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}

	debug_print(k_print_error, "Caught exception!\n");

	HANDLE file = CreateFile(L"ga2022-crash.dmp", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION mini_exception = { 0 };
		mini_exception.ThreadId = GetCurrentThreadId();
		mini_exception.ExceptionPointers = info;
		mini_exception.ClientPointers = FALSE;

		MiniDumpWriteDump(GetCurrentProcess(),
			GetCurrentProcessId(),
			file,
			MiniDumpWithThreadInfo,
			&mini_exception,
			NULL,
			NULL);

		CloseHandle(file);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

void debug_install_exception_handler()
{
	AddVectoredExceptionHandler(TRUE, debug_exception_handler);
}

void debug_set_print_mask(uint32_t mask)
{
	s_mask = mask;
}

void debug_print(uint32_t type, _Printf_format_string_ const char* format, ...)
{
	if ((s_mask & type) == 0)
	{
		return;
	}

	va_list args;
	va_start(args, format);
	char buffer[256];
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	OutputDebugStringA(buffer);

	DWORD bytes = (DWORD)strlen(buffer);
	DWORD written = 0;
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleA(out, buffer, bytes, &written, NULL);
}

int debug_backtrace(void** stack, int stack_capacity)
{
	return CaptureStackBackTrace(1, stack_capacity, stack, NULL);

}

void callstack_printer(uint32_t type, void* stack[], size_t count) {

	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);
	char symbol_mem[sizeof(IMAGEHLP_SYMBOL64) + 256];
	IMAGEHLP_SYMBOL64* symbol = (IMAGEHLP_SYMBOL64*)symbol_mem;
	symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
	symbol->MaxNameLength = 255;
	
	char line_mem[sizeof(IMAGEHLP_LINE64)];
	IMAGEHLP_LINE* line = (IMAGEHLP_LINE*)line_mem;
	line->SizeOfStruct = sizeof(IMAGEHLP_LINE);

	int i;
	for (i = 0; i < count; i++) {
		void* addr = stack[i];
		if (addr == NULL) {
			break;
		}
		SymGetSymFromAddr(process, (DWORD64)addr, 0, symbol);
		DWORD displace = 0;
		SymGetLineFromAddr(process, (DWORD64)addr, &displace, line);
		char* fileName = strrchr(line->FileName, '\\') + 1;
		char* name[256];
		UnDecorateSymbolName(symbol->Name, (PSTR)name, 256, UNDNAME_COMPLETE);
		char buffer[256];
		sprintf_s(buffer, 256, "%s", symbol->Name);
		if (!strcmp(buffer, "invoke_main")) {
			break;
		}
		debug_print(type, "[%d] %s\t\tat %s:%d\n", i, symbol->Name, fileName, line->LineNumber);
		
		
		

		if (!strcmp(buffer,"main")) {
			break;
		}
	}
	SymCleanup(process);
}
