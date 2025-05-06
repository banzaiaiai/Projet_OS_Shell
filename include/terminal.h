// vérification
#ifndef TERMINAL_H
#define TERMINAL_H

#include <errno.h>
#include <stdio.h>
#define _OPEN_SYS
#include <ctype.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

// biblotheque personnel
#include "copy.h"

// Structure
/* A process is a single process.  */
typedef struct process {
  struct process *next; /* next process in pipeline */
  char **argv;          /* for exec */
  pid_t pid;            /* process ID */
  char completed;       /* true if process has completed */
  char stopped;         /* true if process has stopped */
  int status;           /* reported status value */
  int taille;
} process;

/* A job is a pipeline of processes.  */
typedef struct job {
  struct job *next;          /* next active job */
  char *command;             /* command line, used for messages */
  process *first_process;    /* list of processes in this job */
  pid_t pgid;                /* process group ID */
  char notified;             /* true if user told about stopped job */
  struct termios tmodes;     /* saved terminal modes */
  int stdin, stdout, stderr; /* standard i/o channels */
  int background;            /* is job running in background? */
} job;

// Variable globale
extern pid_t shell_pgid;
extern struct termios shell_tmodes;
extern int shell_terminal;
extern int shell_is_interactive;
/* The active jobs are linked into a list.  This is its head.   */
extern job *first_job;

void format_job_info(job *j, const char *status);

int mark_process_status(pid_t pid, int status);

int job_is_stopped(job *j);

int job_is_completed(job *j);

void wait_for_job(job *j);

void put_job_in_foreground(job *j, int cont);

void put_job_in_background(job *j, int cont);

void init_shell();

void launch_process(process *p, pid_t pgid, int infile, int outfile,
                    int errfile, int foreground);

void launch_job(job *j, int foreground);

void check_jobs_status();

void list_jobs();

job *find_job_by_pid(pid_t pid);

void do_fg(char *arg);

void do_bg(char *arg);

#endif
