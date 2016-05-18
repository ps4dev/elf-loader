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
#include <ps4/kern.h>

#include <elfloader.h>

#include "main.h"

typedef struct ElfKernelProcessInformation
{
	struct proc *process;
	uint8_t *processMain;
	uint8_t *main;
	int argc;
	char *argv[3];
	char elfName[128];
	void *processFree;
	void *processMemoryType;
	void *processExit;
}
ElfKernelProcessInformation;

//extern void elfPayloadProcessMain(void *arg);
//extern int elfPayloadProcessMainSize;

typedef void (*ElfProcessExit)(int ret);
typedef void (*ElfProcessFree)(void *m, void *t);
void elfPayloadProcessMain(void *arg)
{
	ElfKernelProcessInformation *pargs = arg;
	//ElfProcessExit pexit = (ElfProcessExit)pargs->processExit;
	((ElfMain)pargs->main)(pargs->argc, pargs->argv);
	((ElfProcessFree)pargs->processFree)(pargs, pargs->processMemoryType);
	//pexit(0); //FIXME: Hmm? Oo -> panics, should not, example sys/dev/mmc/mmcsd.c
}

int elfLoaderKernMain(struct thread *td, void *uap)
{
	ElfRunKernelArgument *arg;
	ElfKernelProcessInformation *pargs;
	char buf[32];
	Elf *elf;
	int isProcess, r;
	int elfPayloadProcessMainSize = 64; // adapt if needed

	arg = (ElfRunKernelArgument *)uap;
	if(arg == NULL || arg->data == NULL)
		return PS4_ERROR_ARGUMENT_MISSING;

	isProcess = arg->isProcess;
 	elf = elfCreateLocalUnchecked((void *)buf, arg->data, arg->size);

	pargs = ps4KernMemoryMalloc(sizeof(ElfKernelProcessInformation) + elfPayloadProcessMainSize + elfMemorySize(elf));
	if(pargs == NULL)
	{
		ps4KernMemoryFree(arg->data);
		ps4KernMemoryFree(arg);
		return PS4_KERN_ERROR_OUT_OF_MEMORY;
	}

	// memory = (info, procmain, main)
	pargs->processMain = (uint8_t *)pargs + sizeof(ElfKernelProcessInformation);
	pargs->main = pargs->processMain + elfPayloadProcessMainSize;
	r = elfLoaderLoad(elf, pargs->main, pargs->main); // delay error return til cleanup
	pargs->main += elfEntry(elf);

	// aux
	pargs->argc = 1;
	pargs->argv[0] = pargs->elfName;
	pargs->argv[1] = NULL;
	pargs->argv[2] = NULL;
	strcpy(pargs->elfName, "kernel-elf");

	// Free user argument
	ps4KernMemoryFree(arg->data);
	ps4KernMemoryFree(arg);

	if(r != ELF_LOADER_RETURN_OK)
	{
		ps4KernMemoryFree(pargs);
		return r;
	}

	if(!isProcess)
	{
		int r;
		r = ((ElfMain)pargs->main)(pargs->argc, pargs->argv);
		ps4KernMemoryFree(pargs);
		ps4KernThreadSetReturn0(td, (register_t)r);
		return PS4_OK;
	}

	pargs->processFree = ps4KernDlSym("free");
	pargs->processMemoryType = ps4KernDlSym("M_TEMP");
	pargs->processExit = ps4KernDlSym("kproc_exit");
	ps4KernMemoryCopy((void *)elfPayloadProcessMain, pargs->processMain, elfPayloadProcessMainSize);

	if(kproc_create((ElfProcessMain)pargs->processMain, pargs, &pargs->process, 0, 0, "ps4sdk-elf-%p", pargs) != 0)
	{
		ps4KernMemoryFree(pargs);
		return PS4_KERN_ERROR_KPROC_NOT_CREATED;
	}

	ps4KernThreadSetReturn0(td, (register_t)pargs->process); // FIXME: Races against free
	return PS4_OK; //FIXME: This does not return 0 Oo?
}
