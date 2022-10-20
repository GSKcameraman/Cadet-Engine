#include "trace.h"
#include "heap.h"
#include "fs.h"
#include "timer.h"
#include "mutex.h"

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct event_t 
{
	char name[256];
	uint64_t pid;
	uint64_t tid;
	event_t* next;
} event_t;


typedef struct trace_t 
{
	
	int capacity;
	int event_num;
	event_t* events;
	heap_t* heap;
	fs_t* fs;
	mutex_t* mutex;
	char path[1024];
	char info[5120];
	bool started;
} trace_t;

trace_t* trace_create(heap_t* heap, int event_capacity)
{
	trace_t* result = heap_alloc(heap, sizeof(trace_t), 8);
	result->capacity = event_capacity;
	result->started = false;
	result->events = NULL;
	result->event_num = 0;
	result->heap = heap;
	result->fs = fs_create(heap, 100);
	result->mutex = mutex_create();
	return result;
}

void trace_destroy(trace_t* trace)
{
	//free every event(if any!)
	event_t* eventa = trace->events;
	while (eventa != NULL) {
		event_t* b = eventa->next;
		heap_free(trace->heap, eventa);
		eventa = b;
	}
	
	mutex_destroy(trace->mutex);
	fs_destroy(trace->fs);
	heap_free(trace->heap, trace);
}

void trace_duration_push(trace_t* trace, const char* name)
{
	if (trace->started) {
		uint64_t tick = timer_get_ticks();
		uint64_t us = timer_ticks_to_us(tick);
		mutex_lock(trace->mutex);
		if (trace->event_num < trace->capacity) {
			event_t* current = heap_alloc(trace->heap, sizeof(event_t), 8);
			strcpy_s(current->name, sizeof(current->name), name);
			current->pid = GetCurrentProcessId();
			current->tid = GetCurrentThreadId();
			current->next = trace->events;
			trace->events = current;
			trace->event_num++;
			//finished making the trace. write to the buffer.
			char buffer[512];
			sprintf_s(buffer, 512, "\n\t\t{\"name\": \"%s\",\"ph\" : \"B\",\"pid\" : %" PRIu64 ",\"tid\" : \"%"PRIu64"\",\"ts\" : %" PRIu64 " },", name, current->pid, current->tid, us);
			strcat_s(trace->info, sizeof(trace->info), buffer);

		}
		mutex_unlock(trace->mutex);
	}
}

void trace_duration_pop(trace_t* trace)
{
	if (trace->started) {
		uint64_t tick = timer_get_ticks();
		uint64_t us = timer_ticks_to_us(tick);
		mutex_lock(trace->mutex);
		event_t* popping = trace->events;
		if (popping != NULL) {
			trace->events = popping->next;
			trace->event_num--;
			char buffer[512];
			sprintf_s(buffer, 512, "\n\t\t{\"name\": \"%s\",\"ph\" : \"E\",\"pid\" : %" PRIu64 ",\"tid\" : \"%"PRIu64"\",\"ts\" : %" PRIu64 " },", popping->name, popping->pid, popping->tid, us);
			strcat_s(trace->info, sizeof(trace->info), buffer);
			heap_free(trace->heap, popping);

		}
		mutex_unlock(trace->mutex);
	}
}

void trace_capture_start(trace_t* trace, const char* path)
{
	trace->started = true;
	strcpy_s(trace->path, sizeof(trace->path), path);
	char* write = "{\n\t\"displayTimeUnit\": \"ns\", \"traceEvents\" : [";
	strcpy_s(trace->info, sizeof(trace->info), write);
}

void trace_capture_stop(trace_t* trace)
{
	trace->started = false;
	//ready the output file
	trace->info[strlen(trace->info) - 1] = '\0';
	char* final = "\n\t] \n}";
	strcat_s(trace->info, sizeof(trace->info), final);
	fs_work_t* writework = fs_write(trace->fs, trace->path, trace->info, strlen(trace->info), false);
	fs_work_destroy(writework);
}
