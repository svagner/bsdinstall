/*-
 * Copyright (c) 2013 Stanislav Putrya
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: stable/9/usr.sbin/bsdinstall/distextract/distextract.c 232433 2012-03-03 02:23:09Z nwhitehorn $
 */

#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <archive.h>
#include <dialog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int sync_files(char *rsyncstring);

int
main(void)
{
	char *rsyncstring;
	int retval;

	if (getenv("RSYNC_URI") == NULL) {
		fprintf(stderr, "RSYNC_URI variable is not set\n");
		return (1);
	}


	rsyncstring = strdup(getenv("RSYNC_URI"));

	init_dialog(stdin, stdout);
	dialog_vars.backtitle = __DECONST(char *, "FreeBSD Installer");
	dlg_put_backtitle();

	if (chdir(getenv("BSDINSTALL_CHROOT")) != 0) {
		char error[512];
		sprintf(error, "Could could change to directory %s: %s\n",
		    getenv("BSDINSTALL_DISTDIR"), strerror(errno));
		dialog_msgbox("Error", error, 0, 0, TRUE);
		end_dialog();
		return (1);
	}

	retval = sync_files(rsyncstring);

	end_dialog();

	return (retval);
}

static int
sync_files(char *rsyncstring)
{
	int fd[2], pipepair[2], status, count = 0;
	pid_t childpid;
	char readbuffer[100];
	char *fileprompt, *progress = NULL, *progresstmp = NULL;
	FILE	*read_pipe;
	    FILE    *write_pipe;
	bzero(readbuffer, sizeof(readbuffer));

	pipe(fd);
	if((childpid = fork()) == -1)
	{
	    dialog_msgbox("Error", "Could not ran fork()", 0, 0, TRUE);
	    return 1;
	}
	if(childpid == 0)
	{
		close(fd[0]);
		dup2(fd[1],1);
		dup2(fd[1],2);
		close(fd[1]);
		execlp("/usr/libexec/bsdinstall/rsync-script", "/usr/libexec/bsdinstall/rsync-script", rsyncstring, "/mnt/", (char *)0);
	}
	else
	{
		close(fd[1]);
		while(read(fd[0], readbuffer, sizeof(readbuffer))!=0)
		{

			fileprompt = strtok(readbuffer, "|");
			progresstmp = strtok(NULL, "");
			progress = strtok(progresstmp, "\n");
			if (progress)
				count = atoi(progress);
			else
				count = 0;

			if(pipe(pipepair)!=0) {
				fputs("Cannot create pipe pair.\n", stderr);
				return(-1);
			}
			if((read_pipe=fdopen(pipepair[0], "r"))==NULL) {
				fputs("Cannot open read end of pipe pair.\n", stderr);
				return(-1);
			}
			dialog_state.pipe_input=read_pipe;
			if((write_pipe=fdopen(pipepair[1], "r"))==NULL) {
				fputs("Cannot open read end of pipe pair.\n", stderr);
				return(-1);
			}
			fprintf(write_pipe, "%d\n%c", count, 4);
			fclose(write_pipe);
			dialog_gauge("Cloning system via rsync", "Progress:", 0, 60, count);
			fclose(read_pipe);
			dialog_vars.help_line = __DECONST(char *, fileprompt);
			dlg_put_backtitle();
			bzero(readbuffer, sizeof(readbuffer));
//			dlg_clear();
		}
		waitpid(childpid, &status, 0);
		return status;
	}
	return (0);
}
