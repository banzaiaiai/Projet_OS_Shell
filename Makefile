
# Nom de l'exécutable
TARGET = shell

# Fichiers sources
SRCS = main.c terminal.c

# Fichiers objets (automatiquement dérivés des sources)
OBJS = $(SRCS:.c=.o)

# Compilateur
CC = gcc

# Options de compilation
CFLAGS = -Wall -Wextra -g -lreadline -o3

# Règle par défaut
all: $(TARGET)

# Édition de liens
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compilation des fichiers .c en .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyer les fichiers générés
clean:
	rm -f $(OBJS) $(TARGET)

# Optionnel : forcer re-compilation complète
re: clean all
