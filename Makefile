# Makefile pro IOS projekt 2
# autor: Rudolf Jurišica <xjuris02@stud.fit.vutbr.cz>

EXEC = proj2
CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic -pthread

.PHONY: clean

all: $(EXEC)

$(EXEC): proj2.o
	$(CC) $(CFLAGS) -o proj2 proj2.o

proj2.o: proj2.c
	$(CC) $(CFLAGS) -c proj2.c

clean: 
	rm -f $(EXEC) *.o proj2.out