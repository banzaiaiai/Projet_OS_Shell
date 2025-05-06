# Nom de l'exécutable
TARGET = shell

# Répertoires
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Fichiers sources
SRCS = $(wildcard $(SRCDIR)/*.c)

# Fichiers objets (placés dans obj/)
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

# Compilateur et options
CC = gcc
CFLAGS = -Wall -Wextra -g -I$(INCDIR) -lreadline -o3

# Règle par défaut
all: $(TARGET)

# Edition de lien
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compilation des .c en .o dans obj/
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Créer le dossier obj si nécessaire
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Nettoyage
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Rebuild complet
re: clean all

