#include "terminal.h"
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;
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

  do {
    pid = waitpid(WAIT_ANY, &status, WUNTRACED);
    if (pid == -1) {
      if (errno == ECHILD) {
        fprintf(stderr, "waitpid: No more children\n");
        break;
      }
      perror("waitpid");
      break;
    }
  } while (!mark_process_status(pid, status) && !job_is_stopped(j) &&
           !job_is_completed(j));
}

void put_job_in_foreground(job *j, int cont) {
  /* Put the job into the foreground.  */
  tcsetpgrp(shell_terminal, j->pgid);

  /* Send the job a continue signal, if necessary.  */
  if (cont) {
    if (tcsetattr(shell_terminal, TCSADRAIN, &j->tmodes) == -1) {
      perror("tcsetpgrp to job");
    } else {
      fprintf(stderr, "Foreground control given to PGID %d\n", j->pgid);
    }

    if (kill(-j->pgid, SIGCONT) < 0)
      perror("kill (SIGCONT)");
  }

  /* Wait for it to report.  */
  wait_for_job(j);

  /* Put the shell back in the foreground.  */
  if (tcsetpgrp(shell_terminal, shell_pgid) == -1) {
    perror("tcsetpgrp to shell");
  } else {
    fprintf(stderr, "Terminal control returned to shell\n");
  }

  /* Restore the shell's terminal modes.  */
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
    // signal(SIGCHLD, SIG_IGN);

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

    while (setpgid(pid, pgid) < 0 && errno == EACCES)
      ;

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

  if (strncmp(p->argv[0], "cp", 2) == 0) {
    printf("réussi\n");
    printf("%s\n", p->argv[1]);
    printf("%s\n", p->argv[2]);
    copyDirectory(p->argv[1], p->argv[2]);
  } else {
    execvp(p->argv[0], p->argv);
  }

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

  /* Add job to the job list */
  j->next = first_job;
  first_job = j;

  format_job_info(j, "launched");

  if (!shell_is_interactive)
    wait_for_job(j);
  else if (foreground)
    put_job_in_foreground(j, 0);
  else
    put_job_in_background(j, 0);
}

void check_jobs_status() {
  job *j, *jlast, *jnext;
  process *p;
  pid_t pid;
  int status;

  /* Update status information for child processes */
  do {
    pid = waitpid(WAIT_ANY, &status, WUNTRACED | WNOHANG);
  } while (!mark_process_status(pid, status) && pid > 0);

  /* Check for completed jobs */
  jlast = NULL;
  for (j = first_job; j; j = jnext) {
    jnext = j->next;

    if (job_is_completed(j)) {
      if (jlast)
        jlast->next = jnext;
      else
        first_job = jnext;

      format_job_info(j, "completed");

      /* Free memory associated with the job */
      for (p = j->first_process; p; p = p->next) {
        int i;
        for (i = 0; i < p->taille; i++)
          free(p->argv[i]);
        free(p->argv);
      }
      free(j->command);
      free(j);
    } else if (job_is_stopped(j) && !j->notified) {
      format_job_info(j, "stopped");
      j->notified = 1;
      jlast = j;
    } else
      jlast = j;
  }
}

void list_jobs() {
  job *j;
  int job_num = 1;

  for (j = first_job; j; j = j->next) {
    printf("[%d] %6d ", job_num++, j->pgid);

    if (job_is_completed(j))
      printf("Completed");
    else if (job_is_stopped(j))
      printf("Stopped  ");
    else
      printf("Running  ");

    printf("\t%s\n", j->command);
  }

  if (!first_job) {
    printf("No active jobs\n");
  }
}

// Returns the job corresponding to the (1-based index)
job *find_job_by_number(int job_num) {
  job *j;
  int i = 1;

  for (j = first_job; j; j = j->next, i++) {
    if (i == job_num) {
      return j;
    }
  }

  return NULL;
}

job *find_job_by_pid(pid_t pid) {
  job *j;

  for (j = first_job; j; j = j->next) {
    if (j->pgid == pid) {
      return j;
    }
  }

  return NULL;
}

void do_fg(char *arg) {
  job *j;
  int job_num;

  // If no argument, use the most recent job
  if (!arg || !*arg) {
    j = first_job;
    if (!j) {
      fprintf(stderr, "fg: no current job\n");
      return;
    }
  } else if (arg[0] == '%') {
    // Parse job number (format: %1, %2, etc.)
    job_num = atoi(arg + 1);
    if (job_num <= 0) {
      fprintf(stderr, "fg: invalid job number: %s\n", arg);
      return;
    }

    j = find_job_by_number(job_num);
    if (!j) {
      fprintf(stderr, "fg: %s: no such job\n", arg);
      return;
    }
  } else {
    // Try to interpret as PID
    pid_t pid = atoi(arg);
    if (pid <= 0) {
      fprintf(stderr, "fg: invalid argument: %s\n", arg);
      return;
    }

    j = find_job_by_pid(pid);
    if (!j) {
      fprintf(stderr, "fg: %s: no such process group\n", arg);
      return;
    }
  }

  // Continue the job if it was stopped
  if (job_is_stopped(j)) {
    printf("Continuing %s\n", j->command);
  }

  // Put the job in the foreground
  put_job_in_foreground(j, job_is_stopped(j));
}

void do_bg(char *arg) {
  job *j;
  int job_num;

  // If no argument, use the most recent stopped job
  if (!arg || !*arg) {
    // Find the most recent stopped job
    for (j = first_job; j; j = j->next) {
      if (job_is_stopped(j)) {
        break;
      }
    }

    if (!j) {
      fprintf(stderr, "bg: no stopped jobs\n");
      return;
    }
  } else if (arg[0] == '%') {
    // Parse job number (format: %1, %2, etc.)
    job_num = atoi(arg + 1);
    if (job_num <= 0) {
      fprintf(stderr, "bg: invalid job number: %s\n", arg);
      return;
    }

    j = find_job_by_number(job_num);
    if (!j) {
      fprintf(stderr, "bg: %s: no such job\n", arg);
      return;
    }

    if (!job_is_stopped(j)) {
      fprintf(stderr, "bg: job already in background\n");
      return;
    }
  } else {
    // Try to interpret as PID
    pid_t pid = atoi(arg);
    if (pid <= 0) {
      fprintf(stderr, "bg: invalid argument: %s\n", arg);
      return;
    }

    j = find_job_by_pid(pid);
    if (!j) {
      fprintf(stderr, "bg: %s: no such process group\n", arg);
      return;
    }

    if (!job_is_stopped(j)) {
      fprintf(stderr, "bg: job already in background\n");
      return;
    }
  }

  // Continue the stopped job in the background
  printf("Continuing %s in background\n", j->command);
  put_job_in_background(j, 1);
}
