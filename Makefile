all: obj bin tests

include conf.mk
include build.mk

-include $(DEP)

obj: $(OBJ)
bin: $(BIN)
tests: $(TESTS)

clean:
	@echo cleaning
	@find . -name '*.c.o' -delete
	@find . -type f -executable -delete
	@find . -name '*.d' -delete

%.c.o: %.c
	@$(info CC $<)
	@$(call makedeps,$*.c.d,$<)
	@$(call compile,$@,$<)
	@$(call add-syms,$*.c.d,$@)

test-%: test-%.c.o
	@$(info LD -o $@)
	@$(call link,$@,$< skel.c.o)
	@$(call write-deps, test-$*.d, $@)
	@$(info TEST $(patsubst test-%,%.c, $@))
	@$(shell $@ > /dev/tty)
	@echo

test: 
	@for test in test-*; do [ -x "$$test" ] && "$$test" && echo; done ||true

.PHONY: clean obj bin test
