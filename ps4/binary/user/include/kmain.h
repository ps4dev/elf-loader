#pragma once

#include <ps4/memory.h>

#include <sys/sysent.h>

/* Types */

typedef int (*ElfMain)(int argc, char **argv);

typedef struct ElfRunUserArgument
{
	ElfMain main;
	Ps4MemoryProtected *memory;
}
ElfRunUserArgument;

typedef struct ElfRunKernelArgument
{
	void *mt;
	size_t size;
	void *data;
}
ElfRunKernelArgument;

int elfLoaderRunInKernelKMain(struct thread *td, void *uap);
