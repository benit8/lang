NAME = lang

SRCS = src/buffer.c \
       src/compiler.c \
       src/debug/dump.c \
       src/gc.c \
       src/interpreter.c \
       src/lexer.c \
       src/main.c \
       src/objects.c \
       src/parser.c \
       src/std/array.c \
       src/std/io.c \
       src/std/table.c \
       src/value.c \
       src/vm.c

OBJS = $(SRCS:.c=.o)

CFLAGS += -W -Wall -Wextra
CFLAGS += -Iinclude
CFLAGS += -g3

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) -o $(NAME) $(OBJS) $(LDFLAGS)

clean:
	$(RM) $(OBJS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all $(NAME) clean fclean re
