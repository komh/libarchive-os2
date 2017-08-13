/*-
 * Copyright (c) 2017      KO Myung-Hun
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * A set of compatibility glue for building libarchive on OS/2 platforms.
 */

#if defined(__OS2__)

#include "archive_platform.h"

#include <process.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/builtin.h>

int _std_fcntl(int, int, ...);

/*
 * Extension to support a named pipe.
 */
int
fcntl(int handle, int request, ...)
{
	va_list argPtr;
	int arg;
	ULONG state;
	int ret;

	va_start(argPtr, request);
	arg = va_arg(argPtr, int);
	va_end(argPtr);

	/* First try fcntl() of libc */
	ret = _std_fcntl(handle, request, arg);
	if (ret == -1)
		return (-1);

	/*
	 * Now flag has been set. Set underlying flags of a named pipe.
	 */
	if (DosQueryNPHState(handle, &state) == 0) {
		if (request == F_GETFL) {
			if (state & NP_NOWAIT)
				ret |= O_NONBLOCK;
		}

		if (request == F_SETFL) {
			if (arg & O_NONBLOCK)
				state |= NP_NOWAIT;
			else
				state &= ~NP_NOWAIT;

			/* extract relevant flags */
			state &= NP_NOWAIT | NP_READMODE_MESSAGE;

			if (DosSetNPHState(handle, state) != 0)
				ret = -1;
		}
	}

	return ret;
}

/*
 * pipe() implementation with a named pipe.
 */
int
pipe(int fds[2])
{
	static volatile uint32_t count = 0;

	uint32_t id;
	HPIPE hread;
	HFILE hwrite;
	HEV   hev;
	ULONG action;

	char pipeName[80];
	char semName[80];

	id = __atomic_increment_u32(&count);

	snprintf(pipeName, sizeof(pipeName),
	    "%s\\%x\\%x", LA_PIPE_NAME_BASE, getpid(), id);

	snprintf(semName, sizeof(semName),
	    "%s\\%x\\%x", LA_SEM_NAME_BASE, getpid(), id);

	/*
	 * NP_NOWAIT should be specified, otherwise DosConnectNPipe() blocks.
	 * If you want to change pipes to blocking-mode, then use DosSetNPHState()
	 * after DosConnectNPipe()
	 */
	if (DosCreateNPipe((PCSZ)pipeName, &hread, NP_ACCESS_INBOUND,
	    NP_NOWAIT | NP_TYPE_BYTE | NP_READMODE_BYTE | 1,
	    32768, 32768, 0) != 0)
		return (-1);

	DosConnectNPipe(hread);

	if (DosOpen((PCSZ)pipeName, &hwrite, &action, 0, FILE_NORMAL,
	    OPEN_ACTION_OPEN_IF_EXISTS,
	    OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_WRITEONLY,
	    NULL) != 0)
		goto exit_createnpipe;

	if (DosCreateEventSem((PCSZ)semName, &hev, DC_SEM_SHARED, FALSE) != 0)
		goto exit_open;

	DosSetNPipeSem(hread, (HSEM)hev, hread);
	DosSetNPipeSem(hwrite, (HSEM)hev, hwrite);

	/* Default to blocking mode */
	DosSetNPHState( hread, NP_WAIT | NP_READMODE_BYTE );
	DosSetNPHState( hwrite, NP_WAIT | NP_READMODE_BYTE );

	/*
	 * _imphandle() is not required specifically, because kLIBC imports
	 * native handles automatically if needed. But here use _imphandle()
	 * specifically.
	 */
	fds[0] = _imphandle(hread);
	fds[1] = _imphandle(hwrite);

	fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC);
	fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC);

	setmode(fds[0], O_BINARY);
	setmode(fds[1], O_BINARY);

	return (0);

exit_open:
	DosClose(hwrite);

exit_createnpipe:
	DosClose(hread);

	return (-1);
}

/*
 * wmemcmp() replacement
 */
int wmemcmp(const wchar_t *wcs1, const wchar_t *wcs2, size_t n)
{
	while (n) {
		if (*wcs1 != *wcs2)
			break;

		wcs1++;
		wcs2++;
		n--;
	}

	if (n == 0)
		return 0;

	return *wcs1 - *wcs2;
}

#endif /* __OS2__ */
