include proj.mk
include build.mk

all: $(OBJ) $(BIN) $(TESTS)

-include $(DEP)

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

%-test: %-test.c.o skel.c.o
	@$(info LD -o $@)
	@$(call link,$@,$< skel.c.o)
	@$(call write-deps, $*-test.d, $@)
	@$(info TEST $(patsubst test-%,%, $@))
	@$@
	@echo

.PHONY: clean
