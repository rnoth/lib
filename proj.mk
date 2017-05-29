CC	?= cc
CFLAGS	+= -pipe -I. -D_POSIX_C_SOURCE -std=c99 -pedantic -Wall -Wextra \
	   -fstrict-aliasing -fstrict-overflow \
	   -fdata-sections -ffunction-sections -fno-exceptions \
	   -fno-unwind-tables -fno-asynchronous-unwind-tables \
	   -fno-stack-protector
LDFLAGS += -lc -Wl,--sort-section=alignment -Wl,--sort-common

SRC	:= $(wildcard *.c tests/*.c)
OBJ	:= $(SRC:.c=.c.o)
DEP	:= $(shell find . -name "*.mk")
BIN	:= $(patsubst %.c, %, $(filter %-test.c, $(SRC)))
#TESTS	:= $(patsubst %.c, %-run, $(filter %-test.c, $(SRC)))

#ifndef NDEBUG
CFLAGS	+= -O0 -g -Werror
CFLAGS	+= -Wunreachable-code \
	   -Wno-missing-field-initializers -Wno-unused-parameter \
	   -Warray-bounds
#else
#LDFLAGS += -Wl,--gc-section
#CFLAGS += -O3 -flto
#endif

