CC	?= cc
CFLAGS	+= -pipe -I. -D_POSIX_C_SOURCE=200809 -D_XOPEN_SOURCE=500 -std=c99 -pedantic -Wall -Wextra \
	   -fstrict-aliasing -fstrict-overflow -foptimize-sibling-calls \
	   -fdata-sections -ffunction-sections -fno-exceptions \
	   -fno-unwind-tables -fno-asynchronous-unwind-tables \
	   -fno-stack-protector
LDFLAGS += -lc -Wl,--sort-section=alignment -Wl,--sort-common

SRC	:= $(wildcard *.c */*.c)
OBJ	:= $(SRC:.c=.c.o)
DEP	:= $(wildcard *.d */*.d)
BIN	:= $(patsubst %.c, %, $(filter %-test.c, $(SRC)))
TESTS	:= $(patsubst %.c, %, $(filter test-%.c, $(SRC)))

$(TESTS): skel.c.o

ifndef NDEBUG
CFLAGS	+= -O0 -g -Werror
CFLAGS	+= -Wunreachable-code \
	   -Wno-missing-field-initializers -Wno-unused-parameter \
	   -Warray-bounds -Wno-missing-braces -Wno-parentheses
else
LDFLAGS += -Wl,--gc-section
CFLAGS += -O3
endif
