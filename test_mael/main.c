#include <errno.h>
#include <stdio.h>
#define _²OPEN_SYS
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

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
} job;

// Variable globale
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;
/* The active jobs are linked into a list.  This is its head.   */
job *first_job = NULL;

void format_job_info(job *j, const char *status) {
  fprintf(stderr, "%ld (%s): %s\n", (long)j->pgid, status, j->command);
}
int mark_process_status(pid_t pid, int status) {
  job *j;
  process *p;

  if (pid > 0) {
    /* Update the record for the process.  */
    for (j = first_job; j; j = j->next)
      for (p = j->first_process; p; p = p->next)
        if (p->pid == pid) {
          p->status = status;
          if (WIFSTOPPED(status))
            p->stopped = 1;
          else {
            p->completed = 1;
            if (WIFSIGNALED(status))
              fprintf(stderr, "%d: Terminated by signal %d.\n", (int)pid,
                      WTERMSIG(p->status));
          }
          return 0;
        }
    fprintf(stderr, "No child process %d.\n", pid);
    return -1;
  }

  else if (pid == 0 || errno == ECHILD)
    /* No processes ready to report.  */
    return -1;
  else {
    /* Other weird errors.  */
    perror("waitpid");
    return -1;
  }
}
int job_is_stopped(job *j) {
  process *p;

  for (p = j->first_process; p; p = p->next)
    if (!p->completed && !p->stopped)
      return 0;
  return 1;
}
int job_is_completed(job *j) {
  process *p;

  for (p = j->first_process; p; p = p->next)
    if (!p->completed)
      return 0;
  return 1;
}
void wait_for_job(job *j) {
  int status;
  pid_t pid;

  do
    pid = waitpid(WAIT_ANY, &status, WUNTRACED);
  while (!mark_process_status(pid, status) && !job_is_stopped(j) &&
         !job_is_completed(j));
}

void put_job_in_foreground(job *j, int cont) {
  /* Put the job into the foreground.  */
  tcsetpgrp(shell_terminal, j->pgid);

  /* Send the job a continue signal, if necessary.  */
  if (cont) {
    tcsetattr(shell_terminal, TCSADRAIN, &j->tmodes);
    if (kill(-j->pgid, SIGCONT) < 0)
      perror("kill (SIGCONT)");
  }

  /* Wait for it to report.  */
  wait_for_job(j);

  /* Put the shell back in the foreground.  */
  tcsetpgrp(shell_terminal, shell_pgid);

  /* Restore the shell’s terminal modes.  */
  tcgetattr(shell_terminal, &j->tmodes);
  tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
}
void put_job_in_background(job *j, int cont) {
  /* Send the job a continue signal, if necessary.  */
  if (cont)
    if (kill(-j->pgid, SIGCONT) < 0)
      perror("kill (SIGCONT)");
}
void init_shell() {
  /* See if we are running interactively.  */
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty(shell_terminal);
  if (shell_is_interactive) {
    /* Loop until we are in the foreground.  */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Ignore interactive and job-control signals.  */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    /* Put ourselves in our own process group.  */
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0) {
      perror("Couldn't put the shell in its own process group");
      exit(1);
    }

    /* Grab control of the terminal.  */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save default terminal attributes for shell.  */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}
void launch_process(process *p, pid_t pgid, int infile, int outfile,
                    int errfile, int foreground) {
  pid_t pid;
  if (shell_is_interactive) {
    /* Put the process into the process group and give the process group
       the terminal, if appropriate.
       This has to be done both by the shell and in the individual
       child processes because of potential race conditions.  */
    pid = getpid();
    if (pgid == 0)
      pgid = pid;
    setpgid(pid, pgid);
    if (foreground)
      tcsetpgrp(shell_terminal, pgid);

    /* Set the handling for job control signals back to the default.  */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
  }

  /* Set the standard input/output channels of the new process.  */
  if (infile != STDIN_FILENO) {
    dup2(infile, STDIN_FILENO);
    close(infile);
  }
  if (outfile != STDOUT_FILENO) {
    dup2(outfile, STDOUT_FILENO);
    close(outfile);
  }
  if (errfile != STDERR_FILENO) {
    dup2(errfile, STDERR_FILENO);
    close(errfile);
  }

  /* Exec the new process.  Make sure we exit.  */
  execvp(p->argv[0], p->argv);

  perror("execvp");
  exit(1);
}
void launch_job(job *j, int foreground) {
  process *p;
  pid_t pid;
  int mypipe[2], infile, outfile;

  infile = j->stdin;
  for (p = j->first_process; p; p = p->next) {
    /* Set up pipes, if necessary.  */
    if (p->next) {
      if (pipe(mypipe) < 0) {
        perror("pipe");
        exit(1);
      }
      outfile = mypipe[1];
    } else
      outfile = j->stdout;

    /* Fork the child processes.  */
    pid = fork();
    if (pid == 0)
      /* This is the child process.  */
      launch_process(p, j->pgid, infile, outfile, j->stderr, foreground);
    else if (pid < 0) {
      /* The fork failed.  */
      perror("fork");
      exit(1);
    } else {
      /* This is the parent process.  */
      p->pid = pid;
      if (shell_is_interactive) {
        if (!j->pgid)
          j->pgid = pid;
        setpgid(pid, j->pgid);
      }
    }

    /* Clean up after pipes.  */
    if (infile != j->stdin)
      close(infile);
    if (outfile != j->stdout)
      close(outfile);
    infile = mypipe[0];
  }

  format_job_info(j, "launched");

  if (!shell_is_interactive)
    wait_for_job(j);
  else if (foreground)
    put_job_in_foreground(j, 0);
  else
    put_job_in_background(j, 0);
}
job *makeJob(char *input) {
  job *new_job = malloc(sizeof(job));
  new_job->command = strdup(input);
  new_job->stdin = STDIN_FILENO;
  new_job->stdout = STDOUT_FILENO;
  new_job->stderr = STDERR_FILENO;
  new_job->pgid = 0;
  new_job->notified = 0;
  new_job->next = NULL;
  return new_job;
}
int main(int argc, char *argv[]) {
  init_shell();

  while (true) {
    // printf("mael shell> ");
    // char input[1024];

    // Lire la commande entrée par l'utilisateur
    // if (!fgets(input, sizeof(input), stdin)) {
    //  break; // Fin de fichier (Ctrl+D)
    //}

    char *input = readline("mael shell> ");

    // Handle EOF (Ctrl+D)
    if (!input) {
      printf("\n");
      break;
    }

    // Supprimer le saut de ligne
    // input[strcspn(input, "\n")] = 0;
    if (input[0] != '\0') {
      add_history(input);

      // Vérifier si la commande est "exit"
      if (strcmp(input, "exit") == 0) {
        free(input);
        break;
      }

      // Vérifier si la commande est "cd"
      if (strncmp(input, "cd", 2) == 0) {
        char *dir = input + 3; // Récupérer l'argument après "cd "
        while (*dir == ' ')
          dir++; // Skip any extra spaces

        if (strlen(dir) == 0) {
          chdir(getenv("HOME"));
          continue;
        }
        if (chdir(dir) != 0) {
          perror("chdir");
        }
        free(input);
        continue; // Éviter de créer un job inutile
      }

      // Allouer un job
      job *new_job = malloc(sizeof(job));
      if (!new_job) {
        perror("malloc");
        free(input);
        continue;
      }

      new_job->command = strdup(input);
      new_job->stdin = STDIN_FILENO;
      new_job->stdout = STDOUT_FILENO;
      new_job->stderr = STDERR_FILENO;
      new_job->pgid = 0;
      new_job->notified = 0;
      new_job->next = NULL;

      // Allouer un processus
      process *afteur_process = malloc(sizeof(process));
      process *new_process = malloc(sizeof(process));
      if (!afteur_process) {
        perror("malloc");
        free(new_job->command);
        free(new_job);
        free(input);
        continue;
      }

      char *token;
      char **args = malloc(64 * sizeof(char *));
      int arg_index = 0;

      char *input_copy = strdup(input);
      if (!input_copy) {
        perror("strdup");
        free(new_job->command);
        free(new_job);
        free(input);
        continue;
      }

      process *head = NULL;
      process *tail = NULL;

      token = strtok(input_copy, " ");
      while (token != NULL) {
        if (strcmp(token, "|") == 0) {
          args[arg_index] = NULL;

          process *p = malloc(sizeof(process));
          p->argv = args;
          p->next = NULL;
          p->completed = 0;
          p->stopped = 0;
          p->status = 0;
          p->pid = 0;
          p->taille = arg_index;

          if (head == NULL) {
            head = tail = p;
          } else {
            tail->next = p;
            tail = p;
          }

          args = malloc(64 * sizeof(char *));
          arg_index = 0;
        } else {
          args[arg_index++] = strdup(token);
        }

        token = strtok(NULL, " ");
      }

      // Ajouter le dernier process après la dernière commande
      if (arg_index > 0) {
        args[arg_index] = NULL;
        process *p = malloc(sizeof(process));
        p->argv = args;
        p->next = NULL;
        p->completed = 0;
        p->stopped = 0;
        p->status = 0;
        p->pid = 0;
        p->taille = arg_index;

        if (head == NULL) {
          head = tail = p;
        } else {
          tail->next = p;
          tail = p;
        }
      }

      free(input_copy);
      new_job->first_process = head;
      launch_job(new_job, 1);
    }

    // Free the inpu from readline outside the if statement
    free(input);
  }

  printf("Exiting mael shell...\n");
  return 0;
}
