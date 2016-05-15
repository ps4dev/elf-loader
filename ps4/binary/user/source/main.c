#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ps4/memory.h>
#include <ps4/util.h>
#include <ps4/kernel.h>

#include <kernel.h>

#include <elfloader.h>

#include "main.h"

/* Constants */

#define ELF_SERVER_IO_PORT 5052
#define ELF_SERVER_USER_PORT 5053
#define ELF_SERVER_KERNEL_PORT 5054 // does not work yet!
#define ELF_SERVER_RETRY 20
#define ELF_SERVER_TIMEOUT 1
#define ELF_SERVER_BACKLOG 10

typedef struct ElfServerArgument
{
	volatile int run;
	struct
	{
		pthread_t io;
		pthread_t user;
		pthread_t kernel;
	}
	thread;
	struct
	{
		volatile int io; // use atomic
		volatile int user;
		volatile int kernel;
	}
	server;
}
ElfServerArgument;

/* Globals */

FILE *__stdinp;
FILE *__stdoutp;
FILE *__stderrp;
int __isthreaded;

/* Elf util */

Elf *elfCreateFromSocket(int client)
{
	Elf *elf;
	size_t s;

	void *m = ps4UtilFileAllocateFromDescriptorUnsized(client, &s);
	if(m == NULL)
		return NULL;

	elf = elfCreate(m, s);
	if(elf != NULL && !elfLoaderIsLoadable(elf))
	{
		elfDestroyAndFree(elf);
		elf = NULL;
	}

	return elf;
}

/* IO server */

void *elfLoaderServerIo(void *arg)
{
	ElfServerArgument *a = (ElfServerArgument *)arg;
	int client;

	if((a->server.io = ps4UtilServerCreateEx(ELF_SERVER_IO_PORT, 10, 20, 1)) < 0)
	{
		a->run = -1;
		return NULL;
	}

	if(a->server.io >= 0 && a->server.io < 3)
	{
		// something is wrong and we get a std io fd -> hard-fix with dummies
		ps4UtilStandardIoRedirectPlain(-1);
		a->server.io = ps4UtilServerCreateEx(ELF_SERVER_IO_PORT, 10, 20, 1);
		if(a->server.io >= 0 && a->server.io < 3)
		{
			a->run = -1;
			return NULL;
		}
	}

	a->run = 1;
	while(a->run == 1)
	{
		client = accept(a->server.io, NULL, NULL);
		if(client < 0)
			continue;
		ps4UtilStandardIoRedirectPlain(client);
		close(client);
	}

	return NULL;
}

/* Userland elf accept server */

void *elfLoaderRunInUserMain(void *arg)
{
	ElfRunUserArgument *argument = (ElfRunUserArgument *)arg;
	char *elfName = "elf";
	char *elfArgv[3] = { elfName, NULL, NULL }; // double null term for envp
	int elfArgc = 1;
	int r;

	if(argument == NULL)
		return NULL;

	r = argument->main(elfArgc, elfArgv);
	ps4MemoryProtectedDestroy(argument->memory);
	//ps4MemoryDestroy(argument->memory);
	free(argument);
	printf("return (user): %i\n", r);

	return NULL;
}

int elfLoaderRunInUser(Elf *elf)
{
	pthread_t thread;
	ElfRunUserArgument *argument;
	int r;

	if(elf == NULL)
		return -1;

 	argument = (ElfRunUserArgument *)malloc(sizeof(ElfRunUserArgument));
	if(argument ==  NULL)
	{
		elfDestroyAndFree(elf);
		return -1;
	}

	if(ps4MemoryProtectedCreate(&argument->memory, elfMemorySize(elf)) != 0)
	//if(ps4MemoryCreate(&argument->memory, elfMemorySize(elf)) != PS4_OK)
	{
		free(argument);
		elfDestroyAndFree(elf);
		return -1;
	}

	argument->main = NULL;
	r = elfLoaderLoad(elf, ps4MemoryProtectedGetWritableAddress(argument->memory), ps4MemoryProtectedGetExecutableAddress(argument->memory));
	//r = elfLoaderLoad(elf, ps4MemoryGetAddress(argument->memory), ps4MemoryGetAddress(argument->memory));
	if(r == ELF_LOADER_RETURN_OK)
		argument->main = (ElfMain)((uint8_t *)ps4MemoryProtectedGetExecutableAddress(argument->memory) + elfEntry(elf));
		//argument->main = (ElfMain)((uint8_t *)ps4MemoryGetAddress(argument->memory) + elfEntry(elf));
	elfDestroyAndFree(elf); // we don't need the "file" anymore

	if(argument->main != NULL)
		pthread_create(&thread, NULL, elfLoaderRunInUserMain, argument);
	else
	{
		ps4MemoryProtectedDestroy(argument->memory);
		//ps4MemoryDestroy(argument->memory);
		free(argument);
		return -1;
	}

	return ELF_LOADER_RETURN_OK;
}

void *elfLoaderServerUser(void *arg)
{
	ElfServerArgument *a = (ElfServerArgument *)arg;
	int client;
	Elf *elf;

 	if((a->server.user = ps4UtilServerCreateEx(ELF_SERVER_USER_PORT, 10, 20, 1)) < 0)
		return NULL;

	while(a->run == 1)
	{
		client = accept(a->server.user, NULL, NULL);
		if(client < 0)
			continue;
		elf = elfCreateFromSocket(client);
		close(client);
		if(elf == NULL)
			break;
		elfLoaderRunInUser(elf);
	}
	a->run = 0;

	close(a->server.user);

	return NULL;
}

/* Kernel elf accept server */

void *elfLoaderRunInKernelMain(void *arg)
{
	Elf *elf = (Elf *)arg;
	void *kern_malloc = ps4KernelDlSym("malloc");
	//void *kern_free = ps4KernelDlSym("free");
	void *kern_mt = ps4KernelDlSym("M_TEMP");
	int64_t r = 0;

	ElfRunKernelArgument ua;
	ElfRunKernelArgument *ka = (ElfRunKernelArgument *)ps4KernelCall(kern_malloc, sizeof(ElfRunKernelArgument), (int64_t)kern_mt, 0x0102);
	ua.size = elfGetSize(elf);
	ua.mt = kern_mt;
	ua.data = (void *)ps4KernelCall(kern_malloc, ua.size, (int64_t)kern_mt, 0x0102);
	ps4KernelMemoryCopy(elfGetData(elf), ua.data, ua.size);
	ps4KernelMemoryCopy(&ua, ka, sizeof(ElfRunKernelArgument));

	elfDestroyAndFree(elf); // we dispose of non-kernel data and rebuild the elf in kernel

	ps4KernelExecute((void *)elfLoaderRunInKernelKMain, ka, &r, NULL);
	printf("return (kernel): %i\n", (int)r);

	//ps4KernelCall(kern_free, ua.data, (int64_t)kern_mt);
	//ps4KernelCall(kern_free, ka, (int64_t)kern_mt);

	return NULL;
}

int elfLoaderRunInKernel(Elf *elf)
{
	pthread_t thread;
	if(elf == NULL)
		return -1;
	pthread_create(&thread, NULL, elfLoaderRunInKernelMain, elf);
	return ELF_LOADER_RETURN_OK;
}

void *elfLoaderServerKernel(void *arg)
{
	ElfServerArgument *a = (ElfServerArgument *)arg;
	int client;
	Elf *elf;

 	if((a->server.kernel = ps4UtilServerCreateEx(ELF_SERVER_KERNEL_PORT, 10, 20, 1)) < 0)
		return NULL;

	while(a->run == 1)
	{
		client = accept(a->server.kernel, NULL, NULL);
		if(client < 0)
			continue;
		elf = elfCreateFromSocket(client);
		close(client);
		if(elf == NULL)
			break;
		elfLoaderRunInKernel(elf);
	}
	a->run = 0;

	close(a->server.kernel);

	return NULL;
}

/* Setup and run threads */

int main(int argc, char **argv)
{
	ElfServerArgument argument = {0};

	int libc;
	FILE **__stdinp_address;
	FILE **__stdoutp_address;
	FILE **__stderrp_address;
	int *__isthreaded_address;
	struct sigaction sa;

	// resolve globals needed for standard io
 	libc = sceKernelLoadStartModule("libSceLibcInternal.sprx", 0, NULL, 0, 0, 0);
	sceKernelDlsym(libc, "__stdinp", (void **)&__stdinp_address);
	sceKernelDlsym(libc, "__stdoutp", (void **)&__stdoutp_address);
	sceKernelDlsym(libc, "__stderrp", (void **)&__stderrp_address);
	sceKernelDlsym(libc, "__isthreaded", (void **)&__isthreaded_address);
	__stdinp = *__stdinp_address;
	__stdoutp = *__stdoutp_address;
	__stderrp = *__stderrp_address;
	__isthreaded = *__isthreaded_address;

	// Suppress sigpipe
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigaction(SIGPIPE, &sa, 0);

	// Change standard io buffering
	setvbuf(stdin, NULL, _IOLBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	//FIXME: check pthread_creates
	// Start handling threads
 	// "io" will set run once its set up to ensure it gets fds 0,1 and 2 or fail using run
	pthread_create(&argument.thread.io, NULL, elfLoaderServerIo, &argument);
	while(argument.run == 0)
		sleep(0);
	if(argument.run == -1)
		return EXIT_FAILURE;

	pthread_create(&argument.thread.user, NULL, elfLoaderServerUser, &argument);
	pthread_create(&argument.thread.kernel, NULL, elfLoaderServerKernel, &argument);

	// If you like to stop the threads, best just close/reopen the browser
	// Otherwise send non-elf
	while(argument.run == 1)
		sleep(1);

	shutdown(argument.server.io, SHUT_RDWR);
	shutdown(argument.server.user, SHUT_RDWR);
	shutdown(argument.server.kernel, SHUT_RDWR);
	close(argument.server.io);
	close(argument.server.user);
	close(argument.server.kernel);

	// This does not imply that the started elfs ended
	// Kernel stuff will continue to run
	// User stuff will end on browser close
	pthread_join(argument.thread.io, NULL);
	pthread_join(argument.thread.user, NULL);
	pthread_join(argument.thread.kernel, NULL);

	return EXIT_SUCCESS;
}
