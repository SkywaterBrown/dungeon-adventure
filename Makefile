.RECIPEPREFIX = >
CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -std=c11
LDLIBS   = -lncurses
SRC      = main.c config.c dungeon.c ai.c render.c
OBJ      = $(SRC:.c=.o)
TARGET   = dg

all: $(TARGET)

$(TARGET): $(OBJ)
> $(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c
> $(CC) $(CFLAGS) -c $< -o $@

clean:
> rm -f $(OBJ) $(TARGET)

.PHONY: all clean
