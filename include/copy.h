// vérifciation
#ifndef COPY_H
#define COPY_H

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 1024 // Size of a block for reading/writting
#define _GNU_SOURCE

int copyFile(const char *source, const char *target);

int copyDirectory(const char *source, const char *target);

#endif
