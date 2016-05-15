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
#include <ps4/kernel.h>
#include <ps4/kern.h>

#include <elfloader.h>

#include "main.h"

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

	ps4KernelProtectionAllDisable();

	Elf *elf = elfCreateLocalUnchecked((void *)buf, arg->data, arg->size);
	//uint8_t *executable = ps4KernMemoryMalloc(elfMemorySize(elf));
	uint8_t *writable = malloc(elfMemorySize(elf), mt, 0x0102);
	int entry = elfEntry(elf);

	main = NULL;
	r = elfLoaderLoad(elf, writable, writable);
	free(arg->data, mt);
	free(arg, mt);

	if(r != ELF_LOADER_RETURN_OK)
		return -1;

	main = (ElfMain)(writable + entry);
	ps4KernelProtectionAllEnable();
	r = main(elfArgc, elfArgv);
	//ps4KernMemoryFree(executable);
	free(writable, mt);
	ps4KernThreadSetReturn0(td, r);

	return PS4_OK;
}
