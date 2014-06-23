/**
 * This source file is used to print out a stack-trace when your program
 * segfaults. It is relatively reliable and spot-on accurate.
 *
 * This code is in the public domain. Use it as you see fit, some credit
 * would be appreciated, but is not a prerequisite for usage. Feedback
 * on it's use would encourage further development and maintenance.
 *
 * Due to a bug in gcc-4.x.x you currently have to compile as C++ if you want
 * demangling to work.
 *
 * Please note that it's been ported into my ULS library, thus the check for
 *
 * Author: Jaco Kroon <jaco@kroon.co.za>
 *
 * Copyright (C) 2005 - 2009 Jaco Kroon
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Bug in gcc prevents from using CPP_DEMANGLE in pure "C" */
#if !defined(__cplusplus) && !defined(NO_CPP_DEMANGLE)
#define NO_CPP_DEMANGLE
#endif

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <ucontext.h>
#include <dlfcn.h>
#include "logger.h"
#include "shared_func.h"
#ifndef NO_CPP_DEMANGLE
#include <cxxabi.h>
#ifdef __cplusplus
using __cxxabiv1::__cxa_demangle;
#endif
#endif

#if defined(REG_RIP)
# define SIGSEGV_STACK_IA64
# define REGFORMAT "%016lx"
#elif defined(REG_EIP)
# define SIGSEGV_STACK_X86
# define REGFORMAT "%08x"
#else
# define SIGSEGV_STACK_GENERIC
# define REGFORMAT "%x"
#endif

extern char g_exe_name[256];

void signal_stack_trace_print(int signum, siginfo_t *info, void *ptr)
{
	static const char *si_codes[3] = {"", "SEGV_MAPERR", "SEGV_ACCERR"};

	int i, f = 0;
	ucontext_t *ucontext;
	Dl_info dlinfo;
	void **bp = NULL;
	void *ip = NULL;
	char cmd[256];
	char buff[256];
	char output[8 * 1024];
	char *pCurrent;

	pCurrent = output;
	ucontext = (ucontext_t*)ptr;
	pCurrent += sprintf(pCurrent, "Segmentation Fault!\n");
	pCurrent += sprintf(pCurrent, "\tinfo.si_signo = %d\n", signum);
	pCurrent += sprintf(pCurrent, "\tinfo.si_errno = %d\n", info->si_errno);
	pCurrent += sprintf(pCurrent, "\tinfo.si_code  = %d (%s)\n", \
			info->si_code, si_codes[info->si_code]);
	pCurrent += sprintf(pCurrent, "\tinfo.si_addr  = %p\n", info->si_addr);
	for(i = 0; i < NGREG; i++)
	{
		pCurrent += sprintf(pCurrent, "\treg[%02d] = 0x"REGFORMAT"\n",
			i, ucontext->uc_mcontext.gregs[i]);
	}

#ifndef SIGSEGV_NOSTACK
#if defined(SIGSEGV_STACK_IA64) || defined(SIGSEGV_STACK_X86)
#if defined(SIGSEGV_STACK_IA64)
	ip = (void*)ucontext->uc_mcontext.gregs[REG_RIP];
	bp = (void**)ucontext->uc_mcontext.gregs[REG_RBP];
#elif defined(SIGSEGV_STACK_X86)
	ip = (void*)ucontext->uc_mcontext.gregs[REG_EIP];
	bp = (void**)ucontext->uc_mcontext.gregs[REG_EBP];
#endif

	pCurrent += sprintf(pCurrent, "\tStack trace:\n");
	while(bp && ip)
	{
		const char *symname;
#ifndef NO_CPP_DEMANGLE
		int status;
		char * tmp;
#endif

		if(!dladdr(ip, &dlinfo))
		{
			break;
		}

		symname = dlinfo.dli_sname;

#ifndef NO_CPP_DEMANGLE
		tmp = __cxa_demangle(symname, NULL, 0, &status);

		if (status == 0 && tmp)
		{
			symname = tmp;
		}
#endif

		sprintf(cmd, "addr2line -e %s %p", g_exe_name, ip);
		if (getExecResult(cmd, buff, sizeof(buff)) != 0)
		{
			*buff = '0';
		}

		pCurrent += sprintf(pCurrent, "\t\t% 2d: %p <%s+%lu> (%s in %s)\n",
			++f, ip, symname,
			(unsigned long)ip-(unsigned long)dlinfo.dli_saddr,
			trim_right(buff), dlinfo.dli_fname);


#ifndef NO_CPP_DEMANGLE
		if (tmp)
		{
			free(tmp);
		}
#endif

		if(dlinfo.dli_sname && !strcmp(dlinfo.dli_sname, "main"))
		{
			break;
		}

		ip = bp[1];
		bp = (void**)bp[0];
	}
#else
	pCurrent += sprintf(pCurrent, "\tStack trace (non-dedicated):\n");
	sz = backtrace(bt, 20);
	strings = backtrace_symbols(bt, sz);
	for(i = 0; i < sz; ++i)
	{
		pCurrent += sprintf(pCurrent, "\t\t%s\n", strings[i]);
	}
#endif
	pCurrent += sprintf(pCurrent, "\tEnd of stack trace.\n");
#else
	pCurrent += sprintf(pCurrent, "\tNot printing stack strace.\n");
#endif

	log_it_ex(&g_log_context, LOG_CRIT, output, pCurrent - output);
}

