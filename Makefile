EXEC = ft_ping

CC = clang
# CFLAGS = -Wall -Wextra -Werror

DEBUG_FLAGS = -g -DDEBUG

SRC_DIR = src
OBJ_DIR = obj
INC_DIR = include

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -MMD -MP -c $< -o $@

-include $(DEPS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

format:
	find . -name '*.c' -o -name '*.h' | xargs clang-format -i

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(EXEC)

DEBUG: CFLAGS += $(DEBUG_FLAGS)
DEBUG: fclean all

LEAKS: all
	sudo valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(EXEC) google.com

.PHONY: all clean fclean format DEBUG LEAKS