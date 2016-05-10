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

#include <machine/specialreg.h>

#undef offsetof
#include <kernel.h>
#include <ps4/kernel.h>
#include <ps4/kern.h>

#include <elfloader.h>

#include "kmain.h"

int elfLoaderRunInKernelKMain(struct thread *td, void *uap)
{
	ElfRunKernelArgument *arg = (ElfRunKernelArgument *)uap;
	char buf[32];
	struct malloc_type *mt = arg->mt;
	ElfMain main;
	int r;

	char *elfName = "elf";
	char *elfArgv[3] = { elfName, NULL, NULL }; // double null term for envp
	int elfArgc = 1;

	Elf *elf = elfCreateLocalUnchecked((void *)buf, arg->data, arg->size);
	void *writable = malloc(elfMemorySize(elf), mt, M_ZERO | M_WAITOK);
	void *executable = ps4KernMemoryMalloc(elfMemorySize(elf));
	int entry = elfEntry(elf);

	main = NULL;
	ps4KernRegisterCr0Set(ps4KernRegisterCr0Get() & ~CR0_WP);
	r = elfLoaderLoadKernel(elf, writable, executable);
	ps4KernRegisterCr0Set(ps4KernRegisterCr0Get() | CR0_WP);
	free(arg->data, mt);
	free(arg, mt);

	if(r != ELF_LOADER_RETURN_OK)
		return -1;

	main = (ElfMain)((uint8_t *)executable + entry);
	r = main(elfArgc, elfArgv);
	ps4KernMemoryFree(executable);
	free(writable, mt);

	ps4KernThreadSetReturn0(td, r);

	return PS4_OK;
}
