#include "fs.h"

#include "event.h"
#include "heap.h"
#include "queue.h"
#include "thread.h"
#include "lz4/lz4.h"

#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct fs_t
{
	heap_t* heap;
	queue_t* file_queue;
	thread_t* file_thread;
	queue_t* compress_queue;
	thread_t* compress_thread;
} fs_t;

typedef enum fs_work_op_t
{
	k_fs_work_op_read,
	k_fs_work_op_write,
} fs_work_op_t;

typedef enum fs_compress_op_t
{
	k_fs_work_op_compress,
	k_fs_work_op_decompress,
} fs_compress_op_t;


typedef struct fs_work_t
{
	heap_t* heap;
	fs_work_op_t op;
	char path[1024];
	bool null_terminate;
	fs_compress_op_t use_compression;
	void* buffer;
	size_t size;
	size_t compression_size;
	event_t* done;
	event_t* com_done;
	int result;
	fs_t* fs;
} fs_work_t;

static int file_thread_func(void* user);

static int file_compress_func(void* user);



fs_t* fs_create(heap_t* heap, int queue_capacity)
{
	fs_t* fs = heap_alloc(heap, sizeof(fs_t), 8);
	fs->heap = heap;
	fs->file_queue = queue_create(heap, queue_capacity);
	fs->file_thread = thread_create(file_thread_func, fs);
	fs->compress_queue = queue_create(heap, queue_capacity);
	fs->compress_thread = thread_create(file_compress_func, fs);
	return fs;
}

void fs_destroy(fs_t* fs)
{
	queue_push(fs->file_queue, NULL);
	thread_destroy(fs->file_thread);
	queue_destroy(fs->file_queue);
	queue_push(fs->compress_queue, NULL);
	thread_destroy(fs->compress_thread);
	queue_destroy(fs->compress_queue); 
	heap_free(fs->heap, fs);
}

fs_work_t* fs_read(fs_t* fs, const char* path, heap_t* heap, bool null_terminate, bool use_compression)
{
	fs_work_t* work = heap_alloc(fs->heap, sizeof(fs_work_t), 8);
	work->heap = heap;
	work->op = k_fs_work_op_read;
	strcpy_s(work->path, sizeof(work->path), path);
	work->buffer = NULL;
	work->size = 0;
	work->done = event_create();
	work->com_done = event_create();
	work->result = 0;
	work->null_terminate = null_terminate;
	if (use_compression) {
		work->use_compression = k_fs_work_op_decompress;
	}
	else {
		//set action to opposite to avoid any misuses
		work->use_compression = k_fs_work_op_compress;
	}

	work->fs = fs;
	queue_push(fs->file_queue, work);
	return work;
}

fs_work_t* fs_write(fs_t* fs, const char* path, const void* buffer, size_t size, bool use_compression)
{
	fs_work_t* work = heap_alloc(fs->heap, sizeof(fs_work_t), 8);
	work->heap = fs->heap;
	work->op = k_fs_work_op_write;
	strcpy_s(work->path, sizeof(work->path), path);
	work->buffer = (void*)buffer;
	work->size = size;
	work->done = event_create();
	work->result = 0;
	work->null_terminate = false;
	
	work->com_done = event_create();
	work->fs = fs;
	if (use_compression)
	{
		// HOMEWORK 2: Queue file write work on compression queue!
		work->use_compression = k_fs_work_op_compress;
		queue_push(fs->compress_queue, work);
		event_wait(work->com_done);
		queue_push(fs->file_queue, work);
	}
	else
	{
		//set action to opposite to avoid any misuses
		work->use_compression = k_fs_work_op_decompress;
		event_signal(work->com_done);
		queue_push(fs->file_queue, work);
	}

	return work;
}

bool fs_work_is_done(fs_work_t* work)
{
	return work ? event_is_raised(work->done) : true;
}

void fs_work_wait(fs_work_t* work)
{
	if (work)
	{
		event_wait(work->done);
	}
}

int fs_work_get_result(fs_work_t* work)
{
	fs_work_wait(work);
	return work ? work->result : -1;
}

void* fs_work_get_buffer(fs_work_t* work)
{
	fs_work_wait(work);
	return work ? work->buffer : NULL;
}

size_t fs_work_get_size(fs_work_t* work)
{
	fs_work_wait(work);
	return work ? work->size : 0;
}

void fs_work_destroy(fs_work_t* work)
{
	if (work)
	{
		event_wait(work->done);
		event_destroy(work->done);
		heap_free(work->heap, work);
	}
}

static void file_read(fs_work_t* work)
{
	wchar_t wide_path[1024];
	if (MultiByteToWideChar(CP_UTF8, 0, work->path, -1, wide_path, sizeof(wide_path)) <= 0)
	{
		work->result = -1;
		return;
	}

	HANDLE handle = CreateFile(wide_path, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		work->result = GetLastError();
		return;
	}

	if (!GetFileSizeEx(handle, (PLARGE_INTEGER)&work->size))
	{
		work->result = GetLastError();
		CloseHandle(handle);
		return;
	}

	work->buffer = heap_alloc(work->heap, work->null_terminate ? work->size + 1 : work->size, 8);

	DWORD bytes_read = 0;
	if (!ReadFile(handle, work->buffer, (DWORD)work->size, &bytes_read, NULL))
	{
		work->result = GetLastError();
		CloseHandle(handle);
		return;
	}

	work->size = bytes_read;
	

	CloseHandle(handle);

	if (work->use_compression == k_fs_work_op_decompress)
	{
		// HOMEWORK 2: Queue file read work on decompression queue!
		queue_push(work->fs->compress_queue, work);
		event_wait(work->com_done);
		if (work->null_terminate)
		{
			((char*)work->buffer)[work->size] = 0;
		}
		event_signal(work->done);
	}
	else
	{
		event_signal(work->com_done);
		if (work->null_terminate)
		{
			((char*)work->buffer)[work->size] = 0;
		}
		event_signal(work->done);
	}
}

static void file_write(fs_work_t* work)
{
	wchar_t wide_path[1024];
	if (MultiByteToWideChar(CP_UTF8, 0, work->path, -1, wide_path, sizeof(wide_path)) <= 0)
	{
		work->result = -1;
		return;
	}

	HANDLE handle = CreateFile(wide_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		work->result = GetLastError();
		return;
	}


	if (work->use_compression == k_fs_work_op_compress) {
		DWORD bytes_written = 0;
		if (!WriteFile(handle, work->buffer, (DWORD)work->compression_size, &bytes_written, NULL))
		{
			work->result = GetLastError();
			CloseHandle(handle);
			return;
		}

		work->compression_size = bytes_written;

		heap_free(work->heap, work->buffer);
	}
	else {
		DWORD bytes_written = 0;
		if (!WriteFile(handle, work->buffer, (DWORD)work->size, &bytes_written, NULL))
		{
			work->result = GetLastError();
			CloseHandle(handle);
			return;
		}

		work->size = bytes_written;

	}
	CloseHandle(handle);

	event_signal(work->done);
}

static int file_thread_func(void* user)
{
	fs_t* fs = user;
	while (true)
	{
		fs_work_t* work = queue_pop(fs->file_queue);
		if (work == NULL)
		{
			break;
		}
		
		switch (work->op)
		{
		case k_fs_work_op_read:
			file_read(work);
			break;
		case k_fs_work_op_write:
			file_write(work);
			break;
		}
	}
	return 0;
}



static void file_compress(fs_work_t* work) {
	void* buffer_temp = heap_alloc(work->heap, work->size + sizeof(int), 8);
	int compressed_size = LZ4_compress_default((char*)work->buffer, (char*)buffer_temp + sizeof(int), (int)work->size, (int)work->size);
	
	if (compressed_size != 0) {
		
		*(int*)buffer_temp = (int) work->size;
		work->compression_size = compressed_size + sizeof(int);
		work->buffer = buffer_temp;
	}
	else {
		heap_free(work->heap, buffer_temp);
	}
	//work->size = compressed_size;
	event_signal(work->com_done);
}

static void file_decompress(fs_work_t* work) {
	int decompressed_size = *(int*)(work->buffer);
	char* buffer_temp = heap_alloc(work->heap, work->null_terminate ? (decompressed_size + 1) : decompressed_size, 8);
	char* compressed = (char*)work->buffer + sizeof(int);
	int compressed_size = (int)(work->size - sizeof(int));
	int decompress_size = LZ4_decompress_safe(compressed, buffer_temp, compressed_size, decompressed_size);
	if (decompress_size > 0) {
		work->size = decompress_size;
		heap_free(work->heap, work->buffer);
		work->buffer = buffer_temp;

	}
	else {
		heap_free(work->heap, buffer_temp);
	}
	event_signal(work->com_done);
}

static int file_compress_func(void* user) {


	fs_t* fs = user;

	while (true) {
		fs_work_t* work = queue_pop(fs->compress_queue);
		if (work == NULL) {
			break;
		}

		switch (work->use_compression) {
		case k_fs_work_op_compress:
			file_compress(work);
			break;
		case k_fs_work_op_decompress:
			file_decompress(work);
			break;
		}
	}
	return 0;
}
