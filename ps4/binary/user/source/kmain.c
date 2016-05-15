#define _XOPEN_SOURCE 700
#define __BSD_VISIBLE 1
#define _KERNEL
#define _WANT_UCRED
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kthread.h>

#include <sys/unistd.h>

#include <machine/specialreg.h>

#undef offsetof
#include <ps4/kernel.h>
#include <ps4/kern.h>

#include <elfloader.h>

#include "main.h"

int elfLoaderKernExecuteEx(ElfRunKernelArgument *arg, int *ret)
{
	char buf[32];
	Elf *elf;
	uint8_t *memory;
	int entry, r;
	ElfMain main;

	char *elfName = "elf";
	char *elfArgv[3] = { elfName, NULL, NULL };
	int elfArgc = 1;

 	elf = elfCreateLocalUnchecked((void *)buf, arg->data, arg->size);
	memory = ps4KernMemoryMalloc(elfMemorySize(elf));
	entry = elfEntry(elf);
	r = elfLoaderLoad(elf, memory, memory);
	ps4KernMemoryFree(arg->data);
	ps4KernMemoryFree(arg);

	if(r == ELF_LOADER_RETURN_OK)
	{
		main = (ElfMain)(memory + entry);
		r = main(elfArgc, elfArgv);
		if(ret != NULL)
			*ret = r;
		r = PS4_OK;
	}
	else
		r = EINVAL;

	ps4KernMemoryFree(memory);

	return r;
}

void elfLoaderKernProcessExecute(void *arg)
{
	elfLoaderKernExecuteEx((ElfRunKernelArgument *)arg, NULL);
	kproc_exit(0);
}

int elfLoaderKernExecute(struct thread *td, ElfRunKernelArgument *arg)
{
	int r, ret;
	ret = 0;
 	r = elfLoaderKernExecuteEx(arg, &ret);
	ps4KernThreadSetReturn0(td, ret);
	return r;
}

int elfLoaderKernMain(struct thread *td, void *uap)
{
	struct proc *newp;
	ElfRunKernelArgument *arg = (ElfRunKernelArgument *)uap;

	if(arg->process == 0)
		return elfLoaderKernExecute(td, arg);

	if(kproc_create(elfLoaderKernProcessExecute, uap, &newp, RFPROC | RFNOWAIT | RFCFDG, 0, "ps4sdk-elf") != 0)
	{
		ps4KernelMemoryFree(arg->data);
		ps4KernelMemoryFree(arg);
		return PS4_KERN_ERROR_OUT_OF_MEMORY;
	}

	ps4KernThreadSetReturn0(td, (register_t)newp);
	return PS4_OK;
}
