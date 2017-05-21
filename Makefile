include config.mk

all: obj bin
obj: $(OBJ)
bin: obj $(BIN)

-include $(DEP)

deps	= $(eval deps-undef := $(shell cat $(1:.o=.u))) \
	  $(foreach obj, $(OBJ) \
	  	, $(if $(filter $(deps-undef), $(shell cat $(obj:.o=.f))) \
		  , $(obj)))

resolve = $(eval undef := $(shell cat $(1:.o=.u))) \
	  $(sort $(foreach obj, $(filter-out $1, $(OBJ)) \
	  	   , $(if $(filter $(undef), $(shell cat $(obj:.o=.f))) \
		     , $(obj) $(call deps,$(obj)))))

%.c.o: %.c
	@makedepend -Y -o.c.o -f - $< 2>/dev/null | sed 1,2d > $*.c.d
	@echo CC $<
	@$(CC) $(CFLAGS) -c -o $@ $<
	@nm -u $@ | awk '{ print $$2 }' | sort > $*.c.u
	@nm -g $@ | awk '/0+ [^U]/ { print $$3 }' | sort > $*.c.f

tests/%-test: tests/%-test.c.o skel.c.o
	@$(eval res := $(call resolve, $<))
	@printf '%s: %s\n' $@ '$(res)' > $@.d
	@echo LD -o $@
	@$(CC) $(CFLAGS) -o $@ $< skel.c.o $(res) $(LDFLAGS)
	@$@ || true
	@echo

test:
	@for test in tests/*-test; do "$$test"; echo; done;

clean:
	@rm -f *.o $(NAME) tests/*-test tests/*.o core vgcore.*
	@echo cleaning

clean-test:
	@rm -f tests/*-test

clean-obj:
	@rm -f *.o *.d *.f *.u

.PHONY: clean test all obj bin
