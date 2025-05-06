#include "copy.h"

/**
 * Copies a file from a source path to a target path.
 * This function reads the source file in blocks and writes them to the target
 * file. It also checks that the target file is writable and preserves the
 * access permissions of the source file.
 */
int copyFile(const char *source, const char *target) {
  // File descriptors for the source (read) and target (write) files
  int sourceDescriptor = open(source, O_RDONLY);
  int targetDescriptor = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);

  // Buffer to store file content during transfer
  char buffer[BUFFER_SIZE];
  struct stat sourceAccessControl;
  struct stat targetAccessControl;
  ssize_t bytesRead, bytesWritten;

  // Check that the target file is writable
  stat(target, &targetAccessControl);
  if (!(targetAccessControl.st_mode & S_IWUSR)) {
    perror("You can't write the target file");
    return EXIT_FAILURE;
  }

  // Read from the source file block by block and write to the target file
  while ((bytesRead = read(sourceDescriptor, buffer, BUFFER_SIZE)) > 0) {
    bytesWritten = write(targetDescriptor, buffer, bytesRead);
    if (bytesWritten == -1) {
      perror("Error during writing to target file");
      close(sourceDescriptor);
      close(targetDescriptor);
      return EXIT_FAILURE;
    }
  }

  if (bytesRead == -1) {
    perror("Error during reading from source file");
    close(sourceDescriptor);
    close(targetDescriptor);
    return EXIT_FAILURE;
  }

  // Close file descriptors
  close(sourceDescriptor);
  close(targetDescriptor);

  // Copy the access permissions from the source file to the target file
  if (stat(source, &sourceAccessControl) == -1) {
    perror("Error while getting access control of the source file");
    return EXIT_FAILURE;
  }
  chmod(target, sourceAccessControl.st_mode);

  return EXIT_SUCCESS;
}

/**
 * Recursively copies a directory (and its content) from a source path to a
 * target path. If the source is a regular file, it calls `copyFile`. Otherwise,
 * it creates the directory structure and copies each item individually.
 */
int copyDirectory(const char *source, const char *target) {
  struct dirent *entry;
  struct stat fileStat;
  struct stat sourceAccessControl;
  char sourcePath[1024];
  char targetPath[1024];

  // Get information about the source to determine its type
  if (stat(source, &sourceAccessControl) == -1) {
    perror("Error while getting access control of the source directory");
    return EXIT_FAILURE;
  }

  // If it's a regular file, just call copyFile
  if (S_ISREG(sourceAccessControl.st_mode)) {
    return copyFile(source, target);
  }

  // Open the source directory
  DIR *sourceDirectory = opendir(source);
  if (sourceDirectory == NULL) {
    perror("Can't open input directory");
    return EXIT_FAILURE;
  }

  // Create and/or open the target directory
  DIR *targetDirectory = opendir(target);
  if (targetDirectory == NULL) {
    mkdir(target, 0755);
    targetDirectory = opendir(target);
    if (targetDirectory == NULL) {
      perror("Can't copy the directory");
      return EXIT_FAILURE;
    }
  }

  // Loop through the entries in the source directory
  while ((entry = readdir(sourceDirectory)) != NULL) {
    // Skip the "." and ".." entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Build full paths for the source and target entries
    snprintf(sourcePath, sizeof(sourcePath), "%s/%s", source, entry->d_name);
    snprintf(targetPath, sizeof(targetPath), "%s/%s", target, entry->d_name);

    // Get information about the current source entry
    if (stat(sourcePath, &fileStat) == -1) {
      perror("Error retrieving file information");
      closedir(sourceDirectory);
      return EXIT_FAILURE;
    }

    // Recursively copy (either file or subdirectory)
    if (copyDirectory(sourcePath, targetPath) != EXIT_SUCCESS) {
      fprintf(stderr, "Failed to copy %s to %s\n", sourcePath, targetPath);
    } else {
      printf("Successfully copied %s to %s\n", sourcePath, targetPath);
    }
  }

  // Close directory streams
  closedir(sourceDirectory);
  closedir(targetDirectory);

  // Apply source directory permissions to the target directory
  chmod(target, sourceAccessControl.st_mode);

  return EXIT_SUCCESS;
}
