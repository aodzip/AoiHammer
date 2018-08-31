PROJECT = aoihammer
SOURCEDIR = ./src
CC = gcc
CFLAGS = -flto -Wall -Wextra -I $(SOURCEDIR) -g
LINKS = -lpthread 

SOURCES = $(shell find "$(SOURCEDIR)" -name "*.cpp" -o -name "*.c" -o -name "*.s")
OBJECTS = $(patsubst %.s, %.o, $(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(SOURCES))))

all: $(PROJECT)

$(PROJECT) : $(OBJECTS)
	$(CC) -o $(PROJECT) $(OBJECTS) $(CFLAGS) $(LINKS) $(INCLUDES)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	find src -name "*.o"  | xargs rm -f
	rm -f $(PROJECT)
	
#Makefile By Aodzip