CC	?= cc
CFLAGS	+= -g3 -pipe -O3 -std=c99 -pedantic -Wall -Wextra -Werror \
	   -Wno-missing-field-initializers -Wno-unused-parameter \
	   -flto -fstrict-aliasing -fomit-frame-pointer -fdata-sections \
	   -ffunction-sections -fno-exceptions -fno-unwind-tables \
	   -fno-asynchronous-unwind-tables -fno-stack-protector
LDFLAGS += -lc -Wl,--gc-sections -Wl,--sort-section=alignment -Wl,--sort-common

SRC	!= find . -maxdepth 1 -name "*.c"
OBJ	:= $(SRC:.c=.c.o)
TESTS	!= find tests -name "*.c"
#NAME	:= libutil.a

all:: deps.mk $(NAME) $(TESTS:.c=)

-include deps.mk

#libutil.a: $(OBJ)
#	ar crs $(NAME) $+

deps.mk: $(SRC) $(TESTS)
	$(CC) -M $+ | sed -e 's/.o$$/.c.o/' -e 's/\(.*\)-test.c.o/\1-test/' > deps.mk

%.c.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

tests/%-test: tests/%-test.c %.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(OBJ)

clean:
	rm -f *.o $(NAME) tests/*-test
