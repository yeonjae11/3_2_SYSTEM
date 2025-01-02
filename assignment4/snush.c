/*---------------------------------------------------------------------------*/
/* snush.c                                                                   */
/* Author: Yeonjae Kim                                                       */
/* Student Id: 2020-15607                                                    */
/* This source code implements the main logic of a custom shell program.     */
/* It handles command parsing, background process management,                */
/* signal handling, and the execution of commands with or without pipelines. */
/*---------------------------------------------------------------------------*/

#include "util.h"
#include "token.h"
#include "dynarray.h"
#include "execute.h"
#include "lexsyn.h"
#include "snush.h"

/*
        //
        // TODO-start: global variables in snush.c
        //

        You may add global variables for handling background processes

        //
        // TODO-end: global variables in snush.c
        //
*/
int total_bg_cnt;
processGroup *bg_process_groups;
process* removed_proc;

static pid_t completed_pgrps[MAX_COMPLETED_PGRPS];
static sig_atomic_t head = 0;
static sig_atomic_t tail = 0;

/*---------------------------------------------------------------------------*/
/* create_process_group                                                      */
/* Creates a new process group with the given PGID and initial process count.*/
/* Parameters:                                                               */
/*   pgid - The process group ID to assign to the new group.                 */
/*   initial_count - The initial number of processes in the group.           */
/* Return value:                                                             */
/*   Pointer to the newly created process group structure.                   */
/* Reads/Writes:                                                             */
/*   Dynamically allocates memory for the process group structure.           */
/* Global variables affected:                                                */
/*   Increases total_bg_cnt by initial_count. Modifies bg_process_groups.    */
/*---------------------------------------------------------------------------*/
processGroup* create_process_group(pid_t pgid, int initial_count) {
    processGroup *new_group = malloc(sizeof(processGroup));
    if (!new_group) {
        error_print(NULL,PERROR);
        exit(EXIT_FAILURE);
    }
    new_group->pgid = pgid;
    new_group->count = initial_count;
    total_bg_cnt += initial_count;
    new_group->head = NULL;
    new_group->next = bg_process_groups;
    bg_process_groups = new_group;
    return new_group;
}

/*---------------------------------------------------------------------------*/
/* free_process_group_base                                                   */
/* Frees all memory associated with the given process group, including its   */
/* processes and associated data.                                            */
/* Parameters:                                                               */
/*   pgrp - Pointer to the process group to free.                            */
/* Return value:                                                             */
/*   None.                                                                   */
/* Reads/Writes:                                                             */
/*   Frees dynamically allocated memory. Updates completed_pgrps and tail.   */
/* Global variables affected:                                                */
/*   Modifies tail and completed_pgrps.                                      */
/*---------------------------------------------------------------------------*/
void free_process_group_base(processGroup* pgrp){
    process *curr_proc = pgrp->head;
    process *next_proc=NULL;
    while(curr_proc){
        next_proc = curr_proc->next;
        free(curr_proc);
        curr_proc = next_proc;
    }
    sig_atomic_t next_tail = (tail + 1) % MAX_COMPLETED_PGRPS;
    completed_pgrps[tail] = pgrp->pgid;
    tail = next_tail;
    free(pgrp);
    return;
}

/*---------------------------------------------------------------------------*/
/* free_process_group                                                        */
/* Removes the specified process group from the list of active groups and    */
/* frees all associated memory.                                              */
/* Parameters:                                                               */
/*   pgrp - Pointer to the process group to free.                            */
/* Return value:                                                             */
/*   None.                                                                   */
/* Reads/Writes:                                                             */
/*   Frees dynamically allocated memory. Modifies bg_process_groups.         */
/* Global variables affected:                                                */
/*   Updates bg_process_groups.                                              */
/*---------------------------------------------------------------------------*/
void free_process_group(processGroup * pgrp){
    processGroup* curr_group = bg_process_groups;
    processGroup* prev_group = NULL;

    while(curr_group){
        if(curr_group == pgrp){
            if(prev_group == NULL){
                bg_process_groups = curr_group->next;
            }
            else{
                prev_group->next = curr_group->next;
            }
            free_process_group_base(curr_group);
        }
        prev_group = curr_group;
        curr_group = curr_group->next;
    }
}

/*---------------------------------------------------------------------------*/
/* add_process                                                               */
/* Adds a process to the specified process group. Removes the process from   */
/* the list of removed processes if it exists there.                         */
/* Parameters:                                                               */
/*   pgrp - Pointer to the process group to which the process will be added. */
/*   pid - The PID of the process to add.                                    */
/* Return value:                                                             */
/*   None.                                                                   */
/* Reads/Writes:                                                             */
/*   Allocates memory for a new process if needed. Frees removed processes.  */
/* Global variables affected:                                                */
/*   Modifies total_bg_cnt and removed_proc.                                 */
/*---------------------------------------------------------------------------*/
void add_process(processGroup* pgrp, pid_t pid) {
    process* curr_proc = removed_proc;
    process* prev_proc = NULL;
    while(curr_proc){
        if(curr_proc->pid == pid){
            if(prev_proc){
                prev_proc -> next = curr_proc -> next;
            }
            else{
                removed_proc = curr_proc -> next;
            }
            free(curr_proc);
            pgrp->count -= 1;
            total_bg_cnt--;
            if(pgrp->count == 0){
                free_process_group(pgrp);
            }
            return;
        }
        curr_proc = curr_proc->next;
    }
    
    process *new_proc = malloc(sizeof(process));
    if (!new_proc) {
        error_print(NULL,PERROR);
        exit(EXIT_FAILURE);
    }
    new_proc->pid = pid;
    new_proc->pgid = pgrp->pgid;
    new_proc->next = pgrp->head;
    pgrp->head = new_proc;
    return;
}

/*---------------------------------------------------------------------------*/
/* remove_process                                                            */
/* Removes a process with the given PID from its process group or marks it   */
/* as removed if not found. Frees memory if the group becomes empty.         */
/* Parameters:                                                               */
/*   pid - The PID of the process to remove.                                 */
/* Return value:                                                             */
/*   None.                                                                   */
/* Reads/Writes:                                                             */
/*   Frees memory for processes and process groups as needed.                */
/* Global variables affected:                                                */
/*   Updates total_bg_cnt, bg_process_groups, and removed_proc.              */
/*---------------------------------------------------------------------------*/
void remove_process(pid_t pid) {
    processGroup *curr_group = bg_process_groups;
    processGroup *prev_group = NULL;

    while (curr_group) {
        process *curr_proc = curr_group->head;
        process *prev_proc = NULL;
        int found = 0;
        while (curr_proc) {
            if (curr_proc->pid == pid) {
                if (prev_proc) {
                    prev_proc->next = curr_proc->next;
                } else {
                    curr_group->head = curr_proc->next;
                }
                free(curr_proc);
                curr_group->count-=1;
                total_bg_cnt--;
                found = 1;
                break;
            }
            prev_proc = curr_proc;
            curr_proc = curr_proc->next;
        }

        if (found && curr_group->count == 0) {
            if (prev_group) {
                prev_group->next = curr_group->next;
            } else {
                bg_process_groups = curr_group->next;
            }
            free_process_group_base(curr_group);
        }

        if(found) return;
    
        prev_group = curr_group;
        curr_group = curr_group->next;
    }
    process *new_proc = malloc(sizeof(process));
    if (!new_proc) {
        error_print(NULL,PERROR);
        exit(EXIT_FAILURE);
    }
    new_proc->pid = pid;
    new_proc->pgid = -1;
    new_proc->next = removed_proc;
    removed_proc = new_proc;
}

/*---------------------------------------------------------------------------*/
/* cleanup                                                                   */
/* Cleans up all dynamically allocated resources related to background       */
/* process management.                                                       */
/* Parameters:                                                               */
/*   None.                                                                   */
/* Return value:                                                             */
/*   None.                                                                   */
/* Reads/Writes:                                                             */
/*   Frees memory for all process groups and removed processes.              */
/* Global variables affected:                                                */
/*   Resets total_bg_cnt, bg_process_groups, and removed_proc.               */
/*---------------------------------------------------------------------------*/
void cleanup() {
    /*
        //
        // TODO-start: cleanup() in snush.c
        //

        You need to free dynamically allocated data structures

        //
        // TODO-end: cleanup() in snush.c
        //
    */
    processGroup *next_group = NULL;
    while (bg_process_groups) {
        next_group = bg_process_groups -> next;
        free_process_group_base(bg_process_groups);
        bg_process_groups = next_group;
    }
    process *next_proc = NULL;
    while(removed_proc){
        next_proc = removed_proc -> next;
        free(removed_proc);
        removed_proc = next_proc;
    }
    total_bg_cnt = 0;
}

/*---------------------------------------------------------------------------*/
/* check_bg_status                                                           */
/* Prints the status of completed background process groups and updates the  */
/* queue of completed groups.                                                */
/* Parameters:                                                               */
/*   None.                                                                   */
/* Return value:                                                             */
/*   None.                                                                   */
/* Reads/Writes:                                                             */
/*   Reads completed_pgrps. Modifies head to reflect processed groups.       */
/* Global variables affected:                                                */
/*   Updates head.                                                           */
/*---------------------------------------------------------------------------*/
void check_bg_status() {
    //
    // TODO-start: check_bg_status() in snush.c
    //

    while (head != tail) {
        pid_t pgid = completed_pgrps[head];
        head = (head + 1) % MAX_COMPLETED_PGRPS;
        printf("[%d] Done background process group\n", pgid);
    }

    //
    // TODO-end: check_bg_status() in snush.c
    //   
}

/*---------------------------------------------------------------------------*/
/* sigzombie_handler                                                         */
/* Handles the SIGCHLD signal, reaping zombie processes and removing them    */
/* from their respective process groups.                                     */
/* Parameters:                                                               */
/*   signo - The signal number (should be SIGCHLD).                          */
/* Return value:                                                             */
/*   None.                                                                   */
/* Reads/Writes:                                                             */
/*   Calls waitpid to reap child processes.                                  */
/* Global variables affected:                                                */
/*   Updates bg_process_groups and removed_proc.                             */
/*---------------------------------------------------------------------------*/
/* Whenever a child process terminates, this handler handles all zombies. */
static void sigzombie_handler(int signo) {
    pid_t pid;
    int stat;

    if (signo == SIGCHLD) {
        while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
            //
            // TODO-start: sigzombie_handler() in snush.c start 
            //

            remove_process(pid);

            //
            // TODO-end: sigzombie_handler() in snush.c end
            // 
        }

        if (pid < 0 && errno != ECHILD && errno != EINTR) {
            perror("waitpid");
        }
    }

    return;
}
/*---------------------------------------------------------------------------*/
static void shell_helper(const char *in_line) {
    DynArray_T oTokens;

    enum LexResult lexcheck;
    enum SyntaxResult syncheck;
    enum BuiltinType btype;
    int pcount;
    int ret_pgid; // background pid
    int is_background;

    oTokens = dynarray_new(0);
    if (oTokens == NULL) {
        error_print("Cannot allocate memory", FPRINTF);
        exit(EXIT_FAILURE);
    }

    lexcheck = lex_line(in_line, oTokens);
    switch (lexcheck) {
    case LEX_SUCCESS:
        if (dynarray_get_length(oTokens) == 0)
            return;

        /* dump lex result when DEBUG is set */
        dump_lex(oTokens);

        syncheck = syntax_check(oTokens);
        if (syncheck == SYN_SUCCESS) {
            btype = check_builtin(dynarray_get(oTokens, 0));
            if (btype == NORMAL) {
                is_background = check_bg(oTokens);

                pcount = count_pipe(oTokens);

                if (is_background) {
                    if (total_bg_cnt + pcount + 1 > MAX_BG_PRO) {
                        printf("Error: Total background processes "\
                                    "exceed the limit (%d).\n", MAX_BG_PRO);
                        return;
                    }
                }

                if (pcount > 0) {
                    ret_pgid = iter_pipe_fork_exec(pcount, oTokens, 
                                                is_background);
                }
                else {
                    ret_pgid = fork_exec(oTokens, is_background);
                }

                if (ret_pgid > 0) {
                    if (is_background == 1)
                        printf("[%d] Background process group running\n", 
                                    ret_pgid);
                }
                else {
                    printf("Invalid return value "\
                        "of external command execution\n");
                }
            }
            else {
                /* Execute builtin command */
                execute_builtin(oTokens, btype);
            }
        }

        /* syntax error cases */
        else if (syncheck == SYN_FAIL_NOCMD)
            error_print("Missing command name", FPRINTF);
        else if (syncheck == SYN_FAIL_MULTREDOUT)
            error_print("Multiple redirection of standard out", FPRINTF);
        else if (syncheck == SYN_FAIL_NODESTOUT)
            error_print("Standard output redirection without file name", 
                        FPRINTF);
        else if (syncheck == SYN_FAIL_MULTREDIN)
            error_print("Multiple redirection of standard input", FPRINTF);
        else if (syncheck == SYN_FAIL_NODESTIN)
            error_print("Standard input redirection without file name", 
                        FPRINTF);
        else if (syncheck == SYN_FAIL_INVALIDBG)
            error_print("Invalid use of background", FPRINTF);
        break;

    case LEX_QERROR:
        error_print("Unmatched quote", FPRINTF);
        break;

    case LEX_NOMEM:
        error_print("Cannot allocate memory", FPRINTF);
        break;

    case LEX_LONG:
        error_print("Command is too large", FPRINTF);
        break;

    default:
        error_print("lex_line needs to be fixed", FPRINTF);
        exit(EXIT_FAILURE);
    }

    /* Free memories allocated to tokens */
    dynarray_map(oTokens, free_token, NULL);
    dynarray_free(oTokens);
}

/*---------------------------------------------------------------------------*/
/* main                                                                      */
/* The entry point of the shell program. Initializes global variables,       */
/* sets up signal handlers, and enters the command input loop.               */
/* Parameters:                                                               */
/*   argc - The argument count.                                              */
/*   argv - The argument vector.                                             */
/* Return value:                                                             */
/*   int - EXIT_SUCCESS on normal exit.                                      */
/* Reads/Writes:                                                             */
/*   Reads user input from stdin. Writes shell output to stdout and stderr.  */
/* Global variables affected:                                                */
/*   Initializes and updates total_bg_cnt, bg_process_groups,                */
/*                                                          and removed_proc.*/
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
    sigset_t sigset;
    char c_line[MAX_LINE_SIZE + 2];

    atexit(cleanup);

    /* Initialize variables for background processes */
    total_bg_cnt = 0;
    bg_process_groups = NULL;
    removed_proc = NULL;

    /*
        //
        // TODO-start: Initializing in snush.c
        //

         You should initialize or allocate your own global variables 
        for handling background processes

        //
        // TODO-end: Initializing in snush.c
        //

    */

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGCHLD);
    sigaddset(&sigset, SIGQUIT);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
 
    /* Register signal handler */
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, sigzombie_handler);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    error_print(argv[0], SETUP);

    while (1) {
        check_bg_status();
        fprintf(stdout, "%% ");
        fflush(stdout);

        if (fgets(c_line, MAX_LINE_SIZE, stdin) == NULL) {
            printf("\n");
            exit(EXIT_SUCCESS);
        }        
        shell_helper(c_line);
    }

    return 0;
}
/*---------------------------------------------------------------------------*/