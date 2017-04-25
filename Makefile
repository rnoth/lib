CC	?= cc
CFLAGS	+= -pipe -std=c99 -pedantic -Wall -Wextra \
	   -Wno-missing-field-initializers -Wno-unused-parameter \
	   -Warray-bounds \
	   -fstrict-aliasing -fstrict-overflow -fomit-frame-pointer \
	   -fdata-sections -ffunction-sections -fno-exceptions \
	   -fno-unwind-tables -fno-asynchronous-unwind-tables \
	   -fno-stack-protector
LDFLAGS += -lc -Wl,--sort-section=alignment -Wl,--sort-common

SRC	!= find . -maxdepth 1 -name "*.c"
OBJ	:= $(SRC:.c=.c.o)
TESTS	!= find tests -name "*.c"

all:: deps.mk $(NAME) $(TESTS:.c=)

ifndef NDEBUG
CFLAGS += -O0 -ggdb -g3 -Werror
else
LDFLAGS += -Wl,--gc-section
CFLAGS += -O3 -flto
endif

-include deps.mk

deps.mk: $(SRC) $(TESTS)
	$(CC) -M $+ | sed -e 's/\.o/.c.o/' -e 's/\(.*\)-test.c.o/\1-test/' > deps.mk

%.c.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

tests/%-test: tests/%-test.c %.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(OBJ)

clean:
	rm -f *.o $(NAME) tests/*-test core vgcore.*

.PHONY: clean
.SECONDARY: $(OBJ)
