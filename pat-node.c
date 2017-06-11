#include <string.h>
#include <pat.ih>
#include <util.h>

uintptr_t
mk_cat(uintptr_t lef, uintptr_t rit)
{
	struct node *cat = 0;

	cat = calloc(1, sizeof *cat);
	if (!cat) return 0;

	nod_init(tag_node(cat), type_cat, chld(lef, rit));

	return tag_node(cat);
}

uintptr_t
nod_alloc(void)
{
	struct node *ret;

	ret = calloc(1, sizeof *ret);

	return ret ? tag_node(ret) : 0;
}

void
nod_init(uintptr_t dest, enum type ty, uintptr_t chld[static 2])
{
	struct node *nod = to_node(dest);

	nod->type = ty;
	memcpy(nod->chld, chld, sizeof nod->chld);
}

uintptr_t
nod_attach(uintptr_t lef, uintptr_t rit)
{
	uintptr_t cat;
	if (!rit) return lef;
	if (is_node(lef)) {
		to_node(lef)->chld[0] = rit;
		return lef;
	} else {
		cat = mk_cat(lef, rit);
		return cat;
	}
}

void
nod_dtor(uintptr_t u)
{
	struct node *nod;

	if (!u) return;
	if (is_leaf(u)) return;

	nod = to_node(u);

	if (is_node(nod->chld[0])) nod_dtor(nod->chld[0]);
	if (is_node(nod->chld[1])) nod_dtor(nod->chld[1]);

	free(nod);
}

size_t
nod_len(uintptr_t n)
{
	return 0; // XXX
}

enum type
nod_type(uintptr_t u)
{
	if (is_leaf(u)) return type_lit;
	else return to_node(u)->type;
}
