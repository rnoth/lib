#include <pat.ih>
#include <util.h>

bool
is_open(uintptr_t u)
{
	struct node *nod;

	if (is_leaf(u)) return false;
	nod = to_node(u);
	if (nod->type != type_sub) {
		return false;
	}
	return !nod->chld[0];
}

uintptr_t
mk_cat(uintptr_t lef, uintptr_t rit)
{
	struct node *cat = 0;

	cat = calloc(1, sizeof *cat);
	if (cat) {
		cat->type = type_cat;
		cat->chld[0] = lef;
		cat->chld[1] = rit;
		cat->len = nod_len(lef) + nod_len(rit);
	}

	return tag_node(cat);
}

uintptr_t
mk_open(void)
{
	struct node *ret;

	ret = calloc(1, sizeof *ret);
	if (ret) ret->type = type_sub;
	return tag_node(ret);
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
	uintptr_t chld;

	if (!u) return;
	if (is_leaf(u)) return;

	nod = to_node(u);

	arr_foreach(chld, nod->chld) {
		if (is_node(chld)) nod_dtor(chld);
	}

	free(nod);
}

size_t
nod_len(uintptr_t n)
{
	if (is_leaf(n)) return 1;
	return to_node(n)->len;
}

enum type
nod_type(uintptr_t u)
{
	if (is_leaf(u)) return type_leaf;
	else return to_node(u)->type;
}

