include proj.mk

compile     = $(shell $(CC) $(CFLAGS) -c -o $1 $2 || echo false)
makedeps    = $(shell makedepend -Y. -o.c.o -f - $2 2>/dev/null | sed 1,2d > $1)
add-syms    = $(if $(wildcard $2), $(call do-syms,$1,$2))
do-syms     = $(call add-defs,$1,$2) $(call add-undefs,$1,$2)
link        = $(shell $(CC) $(CFLAGS) -o $1 $(call resolve,$2) $3 || echo false)

add-defs    = $(shell printf 'defs-%s := %s\n' "$2" "$(call  make-defs,$2)" >> $1)
add-undefs  = $(shell printf 'undefs-%s := %s\n' "$2" "$(call  make-undefs,$2)" >> $1)

make-defs   = $(eval defs-$1 := $(call scan-defs,$1)) $(defs-$1)
make-undefs = $(eval undefs-$1 := $(call scan-undefs,$1)) $(undefs-$1)

scan-defs   = $(shell nm -g $1 | awk '/^[^ ]+ [^U]/ { print $$3 }' | tr '\n' ' ')
scan-undefs = $(shell nm -u $1 | awk '{ print $$2 }' | tr '\n' ' ')

resolve       = $(eval res := $1) $(call do-resolve,$1) $(res)
do-resolve    = $(foreach obj,$(filter-out $(obj) $(res),$(OBJ)), $(call try-res,$1,$(obj)))
try-res       = $(if $(call depends,$1,$2), $(call add-with-deps,$(obj)))
add-with-deps = $(eval res += $1) $(eval res += $(call do-resolve,$1))
depends       = $(filter $(defs-$2), $(undefs-$1))

add-deps      = $(shell printf '%s: %s\n' $2 "$(res)" >$1)

all: $(OBJ) $(BIN) $(TESTS)

-include $(DEP)

clean:
	@echo cleaning
	@find . -maxdepth 2 -name '*.c.*' -delete
	@find . -type f -maxdepth 2 -executable -delete

%.c.o: %.c
	@$(info CC $<)
	@$(call makedeps,$*.c.mk,$<)
	@$(call compile,$@,$<)
	@$(call add-syms,$*.c.mk,$@)

%-test: %-test.c.o skel.c.o
	@$(info LD -o $@)
	@$(call link,$@,$<,skel.c.o)
	@$(call add-deps, $*-test.mk, $@)
	@$(info TEST $(patsubst test-%,%, $@))
	@$@
	@echo
