CC	?= cc
CFLAGS	+= -g3 -Os -std=c99 -pedantic -Wall -Wextra -Werror -fstrict-aliasing \
	   -fomit-frame-pointer -fdata-sections -ffunction-sections -fno-exceptions \
	   -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
	   -Wa,--noexecstack
LDFLAGS += -lc -Wl,--gc-sections -Wl,--sort-section=alignment -Wl,--sort-common

SRC	!= find . -name "*.c"
OBJ	:= $(SRC:.c=.c.o)
TESTS	!= find tests -name "*.c"
NAME	:= libutil.a

all:: deps.mk $(NAME) $(TESTS:.c=)

-include deps.mk

libutil.a: $(OBJ)
	ar crs $(NAME) $+

deps.mk: $(SRC) $(TESTS)
	$(CC) -M $+ | sed -e 's/.o$$/.c.o/' -e 's/\(.*\)-test.c.o/\1-test/' > deps.mk

%.c.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

tests/%-test: tests/%-test.c $(NAME) %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(NAME)

clean:
	rm -f *.o $(NAME) tests/*-test

pat.c.o: CFLAGS += -Wno-unused-parameter
