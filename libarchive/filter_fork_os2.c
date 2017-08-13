/*-
 * Copyright (c) 2007 Joerg Sonnenberger
 * Copyright (c) 2012 Michihiro NAKAJIMA
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
 */

#include "archive_platform.h"

#if defined(__OS2__)

__FBSDID("$FreeBSD$");

#include <alloca.h>
#include <string.h>
#include <process.h>
#include <unistd.h>

#include "archive.h"
#include "archive_cmdline_private.h"

#include "filter_fork.h"

pid_t
__archive_create_child(const char *cmd, int *child_stdin, int *child_stdout)
{
	pid_t child;
	int stdin_pipe[2], stdout_pipe[2], tmp;
	int saved_stdin, saved_stdout;
	struct archive_cmdline *cmdline;

	cmdline = __archive_cmdline_allocate();
	if (cmdline == NULL)
		goto state_allocated;
	if (__archive_cmdline_parse(cmdline, cmd) != ARCHIVE_OK)
		goto state_allocated;

	if (pipe(stdin_pipe) == -1)
		goto state_allocated;
	if (stdin_pipe[0] == 1 /* stdout */) {
		if ((tmp = dup(stdin_pipe[0])) == -1)
			goto stdin_opened;
		close(stdin_pipe[0]);
		stdin_pipe[0] = tmp;
	}
	if (pipe(stdout_pipe) == -1)
		goto stdin_opened;
	if (stdout_pipe[1] == 0 /* stdin */) {
		if ((tmp = dup(stdout_pipe[1])) == -1)
			goto stdout_opened;
		close(stdout_pipe[1]);
		stdout_pipe[1] = tmp;
	}

	saved_stdin = dup(STDIN_FILENO);
	saved_stdout = dup(STDOUT_FILENO);

	dup2(stdin_pipe[0], STDIN_FILENO);
	dup2(stdout_pipe[1], STDOUT_FILENO);

	child = spawnvp(P_NOWAIT, cmdline->path, cmdline->argv);

	dup2(saved_stdin, STDIN_FILENO);
	dup2(saved_stdout, STDOUT_FILENO);

	close(saved_stdin);
	close(saved_stdout);

	close(stdin_pipe[0]);
	close(stdout_pipe[1]);

	*child_stdin = stdin_pipe[1];
	fcntl(*child_stdin, F_SETFL, O_NONBLOCK);
	*child_stdout = stdout_pipe[0];
	fcntl(*child_stdout, F_SETFL, O_NONBLOCK);
	__archive_cmdline_free(cmdline);

	return child;

stdout_opened:
	close(stdout_pipe[0]);
	close(stdout_pipe[1]);
stdin_opened:
	close(stdin_pipe[0]);
	close(stdin_pipe[1]);
state_allocated:
	__archive_cmdline_free(cmdline);
	return -1;
}

void
__archive_check_child(int in, int out)
{
	PPIPEINFO ppi;
	char *p;
	char semName[80];
	HEV hevIn = (HEV)0;
	HEV hevOut = (HEV)0;
	HMUX hmux;
	SEMRECORD asr[2];
	ULONG user;
	int n;

	n = 0;
	ppi = alloca(sizeof(*ppi) + 80);

	if (in != -1) {
		DosQueryNPipeInfo(in, 1, ppi, sizeof(*ppi) + 80);
		p = ppi->szName + strlen(LA_PIPE_NAME_BASE) + 1;
		snprintf(semName, sizeof(semName), "%s\\%s", LA_SEM_NAME_BASE, p);
		DosOpenEventSem((PCSZ)semName, &hevIn);
		asr[n].hsemCur = (HSEM)hevIn;
		asr[n].ulUser = 1;
		n++;
	}

	if (out != -1) {
		DosQueryNPipeInfo(out, 1, ppi, sizeof(*ppi) + 80);
		p = ppi->szName + strlen(LA_PIPE_NAME_BASE) + 1;
		snprintf(semName, sizeof(semName), "%s\\%s", LA_SEM_NAME_BASE, p);
		DosOpenEventSem((PCSZ)semName, &hevOut);
		asr[n].hsemCur = (HSEM)hevOut;
		asr[n].ulUser = 2;
		n++;
	}

	DosCreateMuxWaitSem(NULL, &hmux, n, asr, DCMW_WAIT_ANY);
	DosWaitMuxWaitSem(hmux, -1, &user);
	DosCloseMuxWaitSem(hmux);

	if (out != -1) {
		DosResetEventSem(hevOut, &user);
		DosCloseEventSem(hevOut);
	}
	if (in != -1) {
		DosResetEventSem(hevIn, &user);
		DosCloseEventSem(hevIn);
	}
}

#endif /* defined(__OS2__) */
