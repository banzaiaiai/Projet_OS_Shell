#include "copy.h"

// Exercice 2.1
int copyFile(const char *source, const char *target) {
  // Descriptors of the input and output file
  int sourceDescriptor = open(source, O_RDONLY);
  int targetDescriptor = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);

  // Variable to store the content of the file
  char buffer[BUFFER_SIZE];
  struct stat sourceAccessControl;
  struct stat targetAccessControl;
  ssize_t bytesRead, bytesWritten;

  // Verify that you can write in target file
  stat(target, &targetAccessControl);
  if (!(targetAccessControl.st_mode & S_IWUSR)) {
    perror("You can't write the target file");
    return EXIT_FAILURE;
  }

  // Read the source file by block and write in the target file
  while ((bytesRead = read(sourceDescriptor, buffer, BUFFER_SIZE)) > 0) {
    bytesWritten = write(targetDescriptor, buffer, bytesRead);
    if (bytesWritten == -1) {
      perror("Error during writting in target file");
      close(sourceDescriptor);
      close(targetDescriptor);
      return EXIT_FAILURE;
    }
  }

  if (bytesRead == -1) {
    perror("Error during the reading of source file");
    close(sourceDescriptor);
    close(targetDescriptor);
    return EXIT_FAILURE;
  }

  // Closing files
  close(sourceDescriptor);
  close(targetDescriptor);

  if (stat(source, &sourceAccessControl) == -1) {
    perror("Error while getting the access of the source file");
    return EXIT_FAILURE;
  }
  chmod(target, sourceAccessControl.st_mode);

  return EXIT_SUCCESS;
}
int copyDirectory(const char *source, const char *target) {
  struct dirent *entry;
  struct stat fileStat;
  struct stat sourceAccessControl;
  char sourcePath[1024];
  char targetPath[1024];

  // Determines wether the path is related to a dir or to a file
  if (stat(source, &sourceAccessControl) == -1) {
    perror("Error while getting the access of the source directory");
    return EXIT_FAILURE;
  }
  // Check if the path is a regular file
  if (S_ISREG(sourceAccessControl.st_mode)) {
    return copyFile(source, target);
  }

  DIR *sourceDirectory = opendir(source);
  if (sourceDirectory == NULL) {
    perror("Can't open input directory");
    return EXIT_FAILURE;
  }

  DIR *targetDirectory = opendir(target);
  if (targetDirectory == NULL) {
    mkdir(target, 0755);
    targetDirectory = opendir(target);
    if (targetDirectory == NULL) {
      perror("Can't copy the directory");
      return EXIT_FAILURE;
    }
  }

  while ((entry = readdir(sourceDirectory)) != NULL) {
    // Ignore the two entries "." and ".."
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Concatenate the full path of the source file
    snprintf(sourcePath, sizeof(sourcePath), "%s/%s", source, entry->d_name);

    // verify if the source file is the right one
    if (stat(sourcePath, &fileStat) == -1) {
      perror("Erreur lors de la récupération des informations sur le fichier");
      closedir(sourceDirectory);
      return EXIT_FAILURE;
    }

    // Concatenate the full path of the target file
    snprintf(targetPath, sizeof(targetPath), "%s/%s", target, entry->d_name);

    // Copy the file
    if (copyDirectory(sourcePath, targetPath) != EXIT_SUCCESS) {
      fprintf(stderr, "Échec de la copie du fichier %s vers %s\n", sourcePath,
              targetPath);
    } else {
      printf("Copie réussie de %s vers %s\n", sourcePath, targetPath);
    }
  }

  closedir(sourceDirectory);
  closedir(targetDirectory);

  // Copy the acces control of the source directory to the target one
  chmod(target, sourceAccessControl.st_mode);

  return EXIT_SUCCESS;
}
