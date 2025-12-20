SRCS_LEXER = lexer.c helpers.c
DIR_LEXER = lexer/

SRCS_PARSER = parser.c
DIR_PARSER = parser/

SRCS_SEMANTIC = semantic.c
DIR_SEMANTIC = semantic/

SRCS_CLEANUP = cleanup.c
DIR_CLEANUP = cleanup/

SRCS_ERROR = error_handler.c
DIR_ERROR = error_handler/

SRCS_VALIDATION = validation.c
DIR_VALIDATION = validation/

SRCS_COMPILE = compile.c
DIR_COMPILE = compile/

SRCS_IR = ir_gen.c ir_print.c ir_symboltable.c
DIR_IR = ir/

SRCS_JIT = jit.c emit.c encoders.c
DIR_JIT = jit/

SRCS = main.c utils.c
SRCS += $(addprefix $(DIR_LEXER), $(SRCS_LEXER)) \
		$(addprefix $(DIR_PARSER), $(SRCS_PARSER)) \
		$(addprefix $(DIR_SEMANTIC), $(SRCS_SEMANTIC)) \
		$(addprefix $(DIR_COMPILE), $(SRCS_COMPILE)) \
		$(addprefix $(DIR_IR), $(SRCS_IR)) \
		$(addprefix $(DIR_JIT), $(SRCS_JIT)) \
		$(addprefix $(DIR_ERROR), $(SRCS_ERROR)) \
		$(addprefix $(DIR_VALIDATION), $(SRCS_VALIDATION)) \
		$(addprefix $(DIR_CLEANUP), $(SRCS_CLEANUP))

SRCS_DIR = srcs/

OBJS_DIR = objs/
OBJS = $(addprefix $(OBJS_DIR), $(SRCS:.c=.o))

DEPS_DIR = deps/
DEPS = $(addprefix $(DEPS_DIR), $(SRCS:.c=.d))

INCS = -I./incs/
CC = cc
CFLAGS += -Wall -Wextra -Werror -MMD -MP $(INCS) $(ARCH_FLAGS)
ARCH_FLAGS = -arch x86_64

NAME = tinyCompile

$(OBJS_DIR)%.o: $(SRCS_DIR)%.c
	@mkdir -p $(dir $@) $(dir $(DEPS_DIR)$*)
	@$(CC) $(CFLAGS) -c $< -o $@ -MF $(DEPS_DIR)$*.d

all: $(NAME)

$(NAME): $(OBJS)
	@$(CC) $(CFLAGS) $(OBJS) -o $(NAME)
	@echo ">> Build OK, executable ./$(NAME)"

-include $(DEPS)

clean:
	@rm -rf $(OBJS_DIR) $(DEPS_DIR)
	@echo ">> Clean OK."

fclean:
	@rm -rf $(OBJS_DIR) $(DEPS_DIR) $(NAME)
	@echo ">> FClean OK."

re: fclean all

.PHONY: all clean fclean re
