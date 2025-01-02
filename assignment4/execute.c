/*---------------------------------------------------------------------------*/
/* execute.c                                                                 */
/* Author: Yeonjae Kim                                                       */
/* Student Id: 2020-15607                                                    */
/* This source code implements command execution, including handling         */
/* redirections, pipelines, and background processes for the custom shell.   */
/*---------------------------------------------------------------------------*/

#include "dynarray.h"
#include "token.h"
#include "util.h"
#include "lexsyn.h"
#include "snush.h"
#include "execute.h"

/*---------------------------------------------------------------------------*/
/* redout_handler                                                            */
/* Handles redirection of standard output to a file.                         */
/* Parameters:                                                               */
/*   fname - Pointer to the file name where output will be redirected.       */
/* Return value:                                                             */
/*   None.                                                                   */
/* Reads/Writes:                                                             */
/*   Opens the file and redirects stdout to it. Writes errors to stderr.     */
/* Global variables affected:                                                */
/*   None.                                                                   */
/*---------------------------------------------------------------------------*/
void redout_handler(char *fname) {
	//
	// TODO-start: redout_handler() in execute.c
	// 
	int fd;

	fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0) {
		error_print(NULL, PERROR);
		exit(EXIT_FAILURE);
	}
	else {
		dup2(fd, STDOUT_FILENO);
		close(fd);
	}
	//
	// TODO-end redout_handler() in execute.c
	// 
}
/*---------------------------------------------------------------------------*/
void redin_handler(char *fname) {
	int fd;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		error_print(NULL, PERROR);
		exit(EXIT_FAILURE);
	}
	else {
		dup2(fd, STDIN_FILENO);
		close(fd);
	}
}
/*---------------------------------------------------------------------------*/
int build_command_partial(DynArray_T oTokens, int start, 
						int end, char *args[]) {
	int i, ret = 0, redin = FALSE, redout = FALSE, cnt = 0;
	struct Token *t;

	/* Build command */
	for (i = start; i < end; i++) {

		t = dynarray_get(oTokens, i);

		if (t->token_type == TOKEN_WORD) {
			if (redin == TRUE) {
				redin_handler(t->token_value);
				redin = FALSE;
			}
			else if (redout == TRUE) {
				redout_handler(t->token_value);
				redout = FALSE;
			}
			else
				args[cnt++] = t->token_value;
		}
		else if (t->token_type == TOKEN_REDIN)
			redin = TRUE;
		else if (t->token_type == TOKEN_REDOUT)
			redout = TRUE;
	}
	args[cnt] = NULL;

#ifdef DEBUG
	for (i = 0; i < cnt; i++)
	{
		if (args[i] == NULL)
			printf("CMD: NULL\n");
		else
			printf("CMD: %s\n", args[i]);
	}
	printf("END\n");
#endif
	return ret;
}
/*---------------------------------------------------------------------------*/
int build_command(DynArray_T oTokens, char *args[]) {
	return build_command_partial(oTokens, 0, 
								dynarray_get_length(oTokens), args);
}
/*---------------------------------------------------------------------------*/
void execute_builtin(DynArray_T oTokens, enum BuiltinType btype) {
	int ret;
	char *dir = NULL;
	struct Token *t1;

	switch (btype) {
	case B_EXIT:
		if (dynarray_get_length(oTokens) == 1) {
			// printf("\n");
			dynarray_map(oTokens, free_token, NULL);
			dynarray_free(oTokens);

			exit(EXIT_SUCCESS);
		}
		else
			error_print("exit does not take any parameters", FPRINTF);

		break;

	case B_CD:
		if (dynarray_get_length(oTokens) == 1) {
			dir = getenv("HOME");
			if (dir == NULL) {
				error_print("cd: HOME variable not set", FPRINTF);
				break;
			}
		}
		else if (dynarray_get_length(oTokens) == 2) {
			t1 = dynarray_get(oTokens, 1);
			if (t1->token_type == TOKEN_WORD)
				dir = t1->token_value;
		}

		if (dir == NULL) {
			error_print("cd takes one parameter", FPRINTF);
			break;
		}
		else {
			ret = chdir(dir);
			if (ret < 0)
				error_print(NULL, PERROR);
		}
		break;

	default:
		error_print("Bug found in execute_builtin", FPRINTF);
		exit(EXIT_FAILURE);
	}
}

/*---------------------------------------------------------------------------*/
/* fork_exec                                                                 */
/* Executes a command by forking a new process.                              */
/* Parameters:                                                               */
/*   oTokens - Dynamic array of tokens representing the command.             */
/*   is_background - 1 if the command should run in the background			 */
/*															, 0 otherwise.   */
/* Return value:                                                             */
/*   Process group ID for background processes, 1 for foreground			 */
/*															, or -1 on error.*/
/* Reads/Writes:                                                             */
/*   Creates a child process to execute the command. Handles redirections.   */
/* Global variables affected:                                                */
/*   May modify bg_process_groups for background processes.                  */
/*---------------------------------------------------------------------------*/
/* Important Notice!! 
	Add "signal(SIGINT, SIG_DFL);" after fork
*/
int fork_exec(DynArray_T oTokens, int is_background) {
	//
	// TODO-START: fork_exec() in execute.c 
	//
	int ret = 1;
	pid_t pid;

	pid = fork();

	if (pid == 0) {
		sigset_t sigset;
		
		if(is_background){;
			if (setpgid(getpid(),getpid()) == -1) {
				error_print(NULL, PERROR);
				exit(EXIT_FAILURE);
			}
		}

		sigemptyset(&sigset);
		sigaddset(&sigset, SIGINT);
		sigprocmask(SIG_UNBLOCK, &sigset, NULL);
		signal(SIGINT, SIG_DFL);
		
		int end = dynarray_get_length(oTokens);

		char **args = malloc((end + 1) * sizeof(char *));
		if (args == NULL) {
			error_print(NULL, PERROR);
			exit(EXIT_FAILURE);
		}

		build_command(oTokens, args);

		if (execvp(args[0], args) == -1) {
			error_print(args[0], PERROR);
			free(args);
			exit(EXIT_FAILURE);
		}
	} else if (pid > 0) {
		if (is_background == 0) {
			int status;
			waitpid(pid, &status, 0);
		} else {
			pid_t pgid = pid;
			ret = pgid;
			processGroup *pgrp = create_process_group(pgid,1);
			add_process(pgrp, pid);
		}
	} else {
		error_print(NULL, PERROR);
		exit(EXIT_FAILURE);
	}

	return ret;
	//
	// TODO-END: fork_exec() in execute.c
	//
}

/*---------------------------------------------------------------------------*/
/* iter_pipe_fork_exec                                                       */
/* Executes a pipeline of commands by creating multiple processes.           */
/* Parameters:                                                               */
/*   pcount - Number of pipes in the command.                                */
/*   oTokens - Dynamic array of tokens representing the command.             */
/*   is_background - 1 if the pipeline should run in the background, 		 */
/*																0 otherwise. */
/* Return value:                                                             */
/*   Process group ID for background processes, 1 for foreground, 			 */
/*															or -1 on error.  */
/* Reads/Writes:                                                             */
/*   Creates child processes for each command in the pipeline. Handles pipes.*/
/* Global variables affected:                                                */
/*   May modify bg_process_groups for background processes.                  */
/*---------------------------------------------------------------------------*/
/* Important Notice!! 
	Add "signal(SIGINT, SIG_DFL);" after fork
*/
int iter_pipe_fork_exec(int pcount, DynArray_T oTokens, int is_background) {
	//
	// TODO-START: fork_exec() in execute.c 
	// 
	int ret = 1;

	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);


	int num_cmds = pcount + 1;
	int pipes[pcount][2];
	pid_t pids[num_cmds];
	int pipe_indices[pcount + 2];
	pipe_indices[0] = -1;

	int length = dynarray_get_length(oTokens);

	int idx = 1;
	for (int i = 0; i < length; i++) {
		struct Token *t = dynarray_get(oTokens, i);
		if (t->token_type == TOKEN_PIPE) {
			pipe_indices[idx++] = i;
		}
	}
	pipe_indices[idx] = length;

	for (int i = 0; i < pcount; i++) {
		if (pipe(pipes[i]) == -1) {
			error_print(NULL, PERROR);
			exit(EXIT_FAILURE);
		}
	}

	pid_t pgid = -1;
	processGroup* pgrp = NULL;

	pid_t pid = fork();
	if (pid == 0) {
		if(is_background){
			if (setpgid(getpid(),getpid()) == -1) {
				error_print(NULL, PERROR);
				exit(EXIT_FAILURE);
			}
		}

		sigprocmask(SIG_UNBLOCK, &sigset, NULL);
		signal(SIGINT, SIG_DFL);

		char *args[pipe_indices[1] - pipe_indices[0]];
		int start = pipe_indices[0] + 1;
		int end = pipe_indices[1];

		build_command_partial(oTokens, start, end, args);

		if (pcount > 0) {
			dup2(pipes[0][1], STDOUT_FILENO);
		}

		for (int j = 0; j < pcount; j++) {
			close(pipes[j][0]);
			close(pipes[j][1]);
		}

		if (execvp(args[0], args) == -1) {
			error_print(args[0], PERROR);
			exit(EXIT_FAILURE);
		}
	} else if (pid > 0) {
		pids[0] = pid;
		pgid = pid;
		ret = pgid;

		if (is_background) {
			pgrp = create_process_group(pgid, num_cmds);
			add_process(pgrp, pid);
		}
	} else {
		error_print(NULL, PERROR);
		exit(EXIT_FAILURE);
	}

	for (int i = 1; i < num_cmds; i++) {
		pid_t pid = fork();
		if (pid == 0) {
			if(is_background){
				if (setpgid(getpid(),pgid) == -1) {
					error_print(NULL, PERROR);
					exit(EXIT_FAILURE);
				}
			}

			sigprocmask(SIG_UNBLOCK, &sigset, NULL);
			signal(SIGINT, SIG_DFL);

			char *args[pipe_indices[i+1] - pipe_indices[i]];
			int start = pipe_indices[i] + 1;
			int end = pipe_indices[i + 1];
			build_command_partial(oTokens, start, end, args);

			if (i > 0) {
				dup2(pipes[i - 1][0], STDIN_FILENO);
			}
			if (i < pcount) {
				dup2(pipes[i][1], STDOUT_FILENO);
			}

			for (int j = 0; j < pcount; j++) {
				close(pipes[j][0]);
				close(pipes[j][1]);
			}

			if (execvp(args[0], args) == -1) {
				error_print(args[0], PERROR);
				exit(EXIT_FAILURE);
			}
		}else if (pid > 0) {
            pids[i] = pid;
			if (is_background) {
				if(pgrp==NULL || pgid == -1){
					error_print("pgrp error",FPRINTF);
					exit(EXIT_FAILURE);
				}
				add_process(pgrp, pid);
			}
        } else {
            error_print(NULL, PERROR);
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < pcount; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if (is_background == 0) {
        for (int i = 0; i < num_cmds; i++) {
            int status;
            waitpid(pids[i], &status, 0);
        }
    }

	return ret;
	//
	// TODO-END: fork_exec() in execute.c
	//
}
/*---------------------------------------------------------------------------*/