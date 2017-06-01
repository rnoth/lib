include proj.mk
include build.mk

all: $(OBJ) $(BIN) $(TESTS)

-include $(DEP)

clean:
	@echo cleaning
	@find . -maxdepth 2 -name '*.c.*' -delete
	@find . -type f -maxdepth 2 -executable -delete

%.c.o: %.c
	@$(info CC $<)
	@$(call makedeps,$*.c.d,$<)
	@$(call compile,$@,$<)
	@$(call add-syms,$*.c.d,$@)

%-test: %-test.c.o skel.c.o
	@$(info LD -o $@)
	@$(call link,$@,$< skel.c.o)
	@$(call add-deps, $*-test.d, $@)
	@$(info TEST $(patsubst test-%,%, $@))
	@$@
	@echo
