#include "parse.h"

job *parse_command(char *input) {
  job *j = malloc(sizeof(job));
  if (!j) {
    perror("malloc");
    return NULL;
  }
  j->command = strdup(input);
  j->stdin = STDIN_FILENO;
  j->stdout = STDOUT_FILENO;
  j->stderr = STDERR_FILENO;
  j->pgid = 0;
  j->notified = 0;
  j->next = NULL;
  j->background = 0;

  char *input_copy = strdup(input);
  if (!input_copy) {
    perror("strdup");
    free(j->command);
    free(j);
    return NULL;
  }

  // Check if command ends with '&' to run in background
  int len = strlen(input_copy);
  if (len > 0 && input_copy[len - 1] == '&') {
    input_copy[len - 1] = '\0';
    j->background = 1;
    // Trim trailing whitespace
    len = strlen(input_copy);
    while (len > 0 && isspace(input_copy[len - 1])) {
      input_copy[len - 1] = '\0';
      len--;
    }
  }

  process *head = NULL;
  process *tail = NULL;
  char *saveptr;
  char *pipe_token = strtok_r(input_copy, "|", &saveptr);

  while (pipe_token) {
    // Trim leading whitespace
    while (*pipe_token && isspace(*pipe_token)) {
      pipe_token++;
    }

    // Skip empty pipe segments
    if (*pipe_token) {
      process *p = malloc(sizeof(process));
      if (!p) {
        perror("malloc");
        // Clean up
        free(input_copy);
        while (head) {
          process *next = head->next;
          free(head);
          head = next;
        }
        free(j->command);
        free(j);
        return NULL;
      }

      p->next = NULL;
      p->completed = 0;
      p->stopped = 0;
      p->status = 0;
      p->pid = 0;

      // Parse arguments for this process
      char *token_copy = strdup(pipe_token);
      char **args = malloc(64 * sizeof(char *));
      if (!args || !token_copy) {
        perror("malloc/strdup");
        // Clean up
        free(token_copy);
        free(p);
        free(input_copy);
        while (head) {
          process *next = head->next;
          free(head);
          head = next;
        }
        free(j->command);
        free(j);
        return NULL;
      }

      int arg_index = 0;
      char *arg_saveptr;
      char *arg = strtok_r(token_copy, " \t", &arg_saveptr);

      bool next_token_is_input = false;
      bool next_token_is_output = false;
      bool next_token_is_output_append = false;
      bool next_token_is_stderr = false;
      bool next_token_is_stderr_append = false;
      bool next_token_is_both = false;

      while (arg && arg_index < 63) {
        if (strcmp(arg, "<") == 0) {
          next_token_is_input = true;
        } else if (strcmp(arg, ">") == 0) {
          next_token_is_output = true;
        } else if (strcmp(arg, ">>") == 0) {
          next_token_is_output_append = true;
        } else if (strcmp(arg, "2>") == 0) {
          next_token_is_stderr = true;
        } else if (strcmp(arg, "2>>") == 0) {
          next_token_is_stderr_append = true;
        } else if (strcmp(arg, "&>") == 0) {
          next_token_is_both = true;
        } else if (next_token_is_input) {
          int fd = open(arg, O_RDONLY);
          if (fd < 0) {
            perror("open <");
          } else {
            j->stdin = fd;
          }
          next_token_is_input = false;
        } else if (next_token_is_output) {
          int fd = open(arg, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd < 0) {
            perror("open >");
          } else {
            j->stdout = fd;
          }
          next_token_is_output = false;
        } else if (next_token_is_output_append) {
          int fd = open(arg, O_WRONLY | O_CREAT | O_APPEND, 0644);
          if (fd < 0) {
            perror("open >>");
          } else {
            j->stdout = fd;
          }
          next_token_is_output_append = false;
        } else if (next_token_is_stderr) {
          int fd = open(arg, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd < 0) {
            perror("open 2>");
          } else {
            j->stderr = fd;
          }
          next_token_is_stderr = false;
        } else if (next_token_is_stderr_append) {
          int fd = open(arg, O_WRONLY | O_CREAT | O_APPEND, 0644);
          if (fd < 0) {
            perror("open 2>>");
          } else {
            j->stderr = fd;
          }
          next_token_is_stderr_append = false;
        } else if (next_token_is_both) {
          int fd = open(arg, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd < 0) {
            perror("open &>");
          } else {
            j->stdout = fd;
            j->stderr = fd;
          }
          next_token_is_both = false;
        } else {
          args[arg_index++] = strdup(arg);
        }

        arg = strtok_r(NULL, " \t", &arg_saveptr);
      }

      args[arg_index] = NULL;
      p->argv = args;
      p->taille = arg_index;

      free(token_copy);

      // Add process to the list
      if (!head) {
        head = tail = p;
      } else {
        tail->next = p;
        tail = p;
      }
    }

    pipe_token = strtok_r(NULL, "|", &saveptr);
  }

  free(input_copy);
  j->first_process = head;

  return j;
}
