/*---------------------------------------------------------------------------*/
/* snush.h                                                                   */
/* Author: Yeonjae Kim                                                       */
/*---------------------------------------------------------------------------*/

#ifndef _SNUSH_H_
#define _SNUSH_H_

/* SIG_UNBLOCK & sigset_t */
#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_BG_PRO 16
#define MAX_FG_PRO 16


/*
        //
        // TODO-start: data structures in snush.h
        //

        You can add your own data structures to manage the background processes
        You can also add macros to manage background processes

        //
        // TODO-end: data structures in snush.h
        //
*/
#define MAX_COMPLETED_PGRPS (MAX_BG_PRO*2)
typedef struct process {
    pid_t pid;
    pid_t pgid;
    struct process *next;
} process;

typedef struct processGroup {
    pid_t pgid;
    int count;
    process *head;
    struct processGroup *next;
} processGroup;

processGroup* create_process_group(pid_t pgid, int initial_count);
void add_process(processGroup* pgrp, pid_t pid);

#endif /* _SNUSH_H_ */