compile     = $(shell $(CC) $(CFLAGS) -c -o $1 $2 || echo false)
makedeps    = $(shell makedepend -Y. -o.c.o -f - $2 2>/dev/null | sed 1,2d > $1)
add-syms    = $(if $(wildcard $2), $(call do-syms,$1,$2))
do-syms     = $(call add-defs,$1,$2) $(call add-undefs,$1,$2)
link        = $(shell $(CC) $(CFLAGS) -o $1 $(call get-deps,$2) $3 || echo false)
test        = $(eval $$(shell $1))

add-defs    = $(shell printf 'defs-%s := %s\n' "$2" "$(call make-defs,$2)" >> $1)
add-undefs  = $(shell printf 'undefs-%s := %s\n' "$2" "$(call make-undefs,$2)" >> $1)

make-defs   = $(eval defs-$1 := $(call scan-defs,$1)) $(defs-$1)
make-undefs = $(eval undefs-$1 := $(call scan-undefs,$1)) $(undefs-$1)

scan-defs   = $(shell nm -g $1 | awk '/^[^ ]+ [^U]/ { print $$3 }')
scan-undefs = $(shell nm -u $1 | awk '{ print $$2 }')

get-defs    = $(foreach obj,$1,$(defs-$(obj)))
get-undefs  = $(foreach obj,$1,$(undefs-$(obj)))

get-deps    = $(call deps-reset,$1) $(call get-deps?,$1) $(res)
deps-reset  = $(eval res := $1) $(eval defs := $(call get-defs,$1))
get-deps?   = $(if $(filter-out $(defs),$(call get-undefs,$1)),$(call get-deps!,$1))
get-deps!   = $(foreach obj, $(filter-out $(res),$(OBJ)), $(call add-if-deps,$(obj),$1))

add-if-deps = $(if $(call depends?,$2,$1), $(call add-dep,$1))
depends?    = $(if $(call conflicts?,$1,$2),,$(call resolves?,$1,$2))
conflicts?  = $(filter $(defs-$2), $(defs) $(call get-defs,$1))
resolves?   = $(filter $(defs-$2), $(filter-out $(defs), $(call get-undefs,$1)))

add-dep     = $(eval res += $1) $(eval defs += $(defs-$1)) $(call get-deps?,$1)

write-deps  = $(shell printf '%s: %s\n' $2 "$(res)" >$1)
