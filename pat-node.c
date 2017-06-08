#include <string.h>
#include <pat.ih>
#include <util.h>

bool
is_open(uintptr_t u)
{
	struct node *nod;

	if (!u) return false;
	if (is_leaf(u)) return false;

	nod = to_node(u);
	if (nod->type != type_sub) return false;

	return !nod->chld[0];
}

bool
is_subexpr(uintptr_t u)
{
	struct node *nod;

	if (!u) return false;
	if (is_leaf(u)) return false;

	nod = to_node(u);
	if (nod->type != type_sub) return false;
	if (!nod->chld[0]) return false;

	return !nod->chld[1];
}

uintptr_t
mk_alt(uintptr_t lef, uintptr_t rit)
{
	struct node *alt = 0;

	alt = calloc(1, sizeof *alt);
	if (!alt) return 0;

	alt->type = type_alt;
	alt->chld[0] = lef;
	alt->chld[1] = rit;

	return tag_node(alt);
}


uintptr_t
mk_cat(uintptr_t lef, uintptr_t rit)
{
	struct node *cat = 0;

	cat = calloc(1, sizeof *cat);
	if (!cat) return 0;

	cat->type = type_cat;
	cat->chld[0] = lef;
	cat->chld[1] = rit;

	return tag_node(cat);
}

uintptr_t
mk_leaf(char ch)
{
	uint8_t *res;

	res = malloc(1);
	if (!res) return 0;

	memcpy(res, &ch, 1);

	return tag_leaf(res);
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
mk_rep(enum type ty, uintptr_t chld)
{
	struct node *ret;

	ret = calloc(1, sizeof *ret);
	if (!ret) return 0;

	ret->type = ty;
	ret->chld[0] = chld;

	return tag_node(ret);
}

uintptr_t
mk_subexpr(uintptr_t chld)
{
	struct node *ret;

	ret = calloc(1, sizeof *ret);
	if (!ret) return 0;

	ret->type = type_sub;
	ret->chld[0] = chld;

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

