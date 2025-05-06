#include "terminal.h"
int main() {
  init_shell();

  /* Enable job control signals */
  signal(SIGCHLD, SIG_DFL);

  while (true) {
    /* Check for and report any terminated jobs */
    check_jobs_status();

    /* Display prompt and read command */
    char *input = readline("mael shell> ");

    if (!input) {
      /* End of file (Ctrl+D) */
      printf("\n");
      break;
    }

    /* Skip empty lines */
    if (input[0] != '\0') {
      add_history(input);

      /* Check for built-in commands */
      if (strcmp(input, "exit") == 0) {
        free(input);
        break;
      } else if (strcmp(input, "jobs") == 0) {
        list_jobs();
        free(input);
        continue;
      } else if (strncmp(input, "fg", 2) == 0) {
        char *arg = input + 2;
        while (*arg && isspace(*arg))
          arg++; // Skip whitespace
        do_fg(arg);
        free(input);
        continue;
      } else if (strncmp(input, "bg", 2) == 0) {
        char *arg = input + 2;
        while (*arg && isspace(*arg))
          arg++; // Skip whitespace
        do_bg(arg);
        free(input);
        continue;
      } else if (strncmp(input, "cd", 2) == 0) {
        char *dir = input + 2;
        while (*dir && isspace(*dir))
          dir++; /* Skip whitespace */

        if (*dir == '\0')
          dir = getenv("HOME");

        if (chdir(dir) != 0)
          perror("chdir");

        free(input);
        continue;
      }

      /* Parse the command */
      job *j = parse_command(input);
      if (j && j->first_process) {
        /* Launch the job */
        launch_job(j, !j->background);
      } else if (j) {
        free(j->command);
        free(j);
      }
    }

    free(input);
  }

  printf("Exiting mael shell...\n");
  return 0;
}
