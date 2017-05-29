include proj.mk

compile     = $(shell $(CC) $(CFLAGS) -c -o $1 $2)
makedeps    = $(shell makedepend -Y. -o.c.o -f - $2 2>/dev/null | sed 1,2d > $1)
add-syms    = $(call add-defs,$1,$2) $(call add-undefs,$1,$2)
link        = $(info $(CC) $(CFLAGS) -o $1 $2 $(call resolve,$2))

add-defs    = $(shell printf 'defs-%s := %s\n' "$2" "$(call  make-defs,$2)" >> $1)
add-undefs  = $(shell printf 'undefs-%s := %s\n' "$2" "$(call  make-undefs,$2)" >> $1)

make-defs   = $(eval defs-$1 := $(call scan-defs,$1)) $(defs-$1)
make-undefs = $(eval undefs-$1 := $(call scan-undefs,$1)) $(undefs-$1)

scan-defs   = $(shell nm -g $1 | awk '/^[^ ]+ [^U]/ { print $$3 }' | tr '\n' ' ')
scan-undefs = $(shell nm -u $1 | awk '{ print $$2 }' | tr '\n' ' ')

resolve     = $(sort $(foreach obj,$(OBJ),$(call if-deps,$1,$(obj), $(obj) $(call deps,$(obj)))))
if-deps     = $(if $(call depends,$1,$2),$(if $(call conflicts,$1,$2),,$3))
conflicts   = $(filter $(defs-$1), $(defs-$2))
depends     = $(filter $(defs-$2), $(undefs-$1)),
deps        = $(foreach obj,$(OBJ),$(call if-deps,$1,$(obj), $(obj)))

all: $(OBJ) $(BIN)

-include $(DEP)

clean:
	find . -name '*.c.*' -delete
	find . -executable -delete

%.c.o: %.c
	@$(info CC $<)
	@$(call makedeps,$*.c.mk,$<)
	@$(call compile,$@,$<)
	@$(call add-syms,$*.c.mk,$@)

test-%: test-%.c.o
	@$(info LD -o $@)
	@$(call link,$@,$<)

run-test-%: test-%
	@$(info TEST $(patsubst test-%,%, $@))
	@$@
