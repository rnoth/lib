#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <util.h>
#include <vec.h>
#include <pat.ih>
#include <pat.h>

enum {
	lef_set,
	lef_end,
	rig_set,
	rig_end,
};

struct frame {
	int (*op)(struct context *, struct thread *, char const);
	uint8_t pos:2;
};

struct gen {
	struct frame beg;
	struct frame end;
};

static int comp_alt(struct ins **, struct node *);
static int comp_cat(struct ins **, struct node *);
static int comp_class(struct ins **, struct node *);
static int comp_chld(struct ins **, uintptr_t);
static int comp_leaf(struct ins **, uintptr_t);
static int comp_rep(struct ins **, struct node *);
static int comp_sub(struct ins **, struct node *);

static int comp_node(struct ins **, uintptr_t);
static ptrdiff_t nod_off(uintptr_t, uint8_t, uint8_t);

static struct gen tab_gen[] = {
	[type_opt] = { {do_fork, lef_end}, {0} },
	[type_rep] = { {0},                {do_fork, lef_set} },
	[type_kln] = { {do_fork, lef_end}, {do_fork, lef_end} },
	[type_alt] = { {do_fork, lef_end}, {do_jump, rig_end} },
	[type_sub] = { {do_mark},          {do_save}, },
};

static int (* const pat_comp[])(struct ins **, struct node *) = {
	[type_alt] = comp_alt,
	[type_cat] = comp_cat,
	[type_cls] = comp_class,
	[type_opt] = comp_rep,
	[type_kln] = comp_rep,
	[type_rep] = comp_rep,
	[type_sub] = comp_sub,
};

size_t
type_len(enum type ty)
{
	return !!tab_gen[ty].beg.op + !!tab_gen[ty].end.op;
}

ptrdiff_t
nod_off(uintptr_t nod, uint8_t dst, uint8_t pos)
{
	size_t off;
	size_t lef;
	size_t rig;

	lef = nod_len(left(nod));
	rig = nod_len(right(nod));

	switch (dst) {
	case lef_set:
		off = 0;
		break;
	case lef_end:
		off = lef;
		break;
	}

	switch (pos) {
	case lef_set: return 0 - off;
	case lef_end:
	case rig_set: return lef - off;
	case rig_end: return lef + rig - off;
	}

	return 0;
}

int
comp_gen(struct ins **dst, uintptr_t nod)
{
	if (is_leaf(nod)) return comp_leaf(dst, nod);
	else return comp_node(dst, nod);
}

int
comp_node(struct ins **dst, uintptr_t nod)
{
	struct frame *fr;
	ptrdiff_t off;
	int err;

	fr = &tab_gen[nod_type(nod)].beg;

	if (fr->op) {
		off = nod_off(nod, fr->pos, lef_set);
		err = vec_append(dst, ((struct ins[]){{ .op = fr->op, .arg.f = off }}));
		if (err) return err;
	}

	err = comp_gen(dst, left(nod));
	if (err) return err;

	fr = &tab_gen[nod_type(nod)].end;

	if (fr->op) {
		off = nod_off(nod, fr->pos, lef_end);
		err = vec_append(dst, ((struct ins[]){{ .op = fr->op, .arg.f = off }}));
		if (err) return err;
	}

	return comp_gen(dst, right(nod));
}

int
comp_alt(struct ins **dest, struct node *alt)
{
	size_t fork;
	size_t jump;
	int err;

	fork = vec_len(*dest);
	err = vec_append(dest, (struct ins[]) {
		{ .op = do_fork }
	});
	if (err) return err;

	err = comp_chld(dest, alt->chld[0]);
	if (err) return err;

	jump = vec_len(*dest);
	err = vec_append(dest, (struct ins[]) {
		{ .op = do_jump }
	});
	if (err) return err;

	dest[0][fork].arg.f = vec_len(*dest) - fork;

	err = comp_chld(dest, alt->chld[1]);
	if (err) return err;

	dest[0][jump].arg.f = vec_len(*dest) - jump;

	return 0;
}

int
comp_cat(struct ins **dest, struct node *nod)
{
	int err = 0;

	err = comp_chld(dest, nod->chld[0]);
	if (err) return err;

	return comp_chld(dest, nod->chld[1]);
}

int
comp_chld(struct ins **dest, uintptr_t n)
{
	if (!n) return 0;
	if (is_leaf(n)) return comp_leaf(dest, n);
	struct node *nod = to_node(n);
	return pat_comp[nod->type](dest, nod);
}

int
comp_leaf(struct ins **dest, uintptr_t lea)
{
	int err;

	err = vec_append(dest, ((struct ins[]) {{
		.op = do_char,
		.arg = {.b = *to_leaf(lea)},
	}}));
	if (err) return err;

	return 0;
}

int
comp_class(struct ins **dest, struct node *nod)
{
	int err;

	err = vec_append(dest, ((struct ins[]) {
		{.op = do_clss, .arg = {.i = *to_leaf(nod->chld[0])}}
	}));
	if (err) return err;

	return comp_chld(dest, nod->chld[1]);
}

int
comp_rep(struct ins **dest, struct node *rep)
{
	size_t beg;
	size_t off;
	int err;

	off = vec_len(*dest);

	if (rep->type != type_rep) {
		err = vec_append(dest, (struct ins[]) {{ .op = do_fork }});
		if (err) return err;
	}

	beg = vec_len(*dest);

	err = comp_chld(dest, rep->chld[0]);
	if (err) return err;

	if (rep->type != type_opt) {
		err = vec_append(dest, ((struct ins[]) {{
			.op = do_fork,
			.arg = { .f = beg - vec_len(*dest) },
		}}));
		if (err) return err;
	}

	if (rep->type != type_rep) {
		(*dest)[off].arg.f = vec_len(*dest) - off;
	}

	return comp_chld(dest, rep->chld[1]);
}

int
comp_sub(struct ins **dest, struct node *root)
{
	int err;

	err = vec_append(dest, ((struct ins[]) {
		{ .op = do_mark, .arg = {0}, },
	}));
	if (err) goto finally;

	err = comp_chld(dest, root->chld[0]);
	if (err) goto finally;

	err = vec_append(dest, ((struct ins[]) {
		{ .op = do_save, .arg = {0}, },
	}));
	if (err) goto finally;

	err = comp_chld(dest, root->chld[1]);
	if (err) goto finally;

finally:
	if (err) vec_truncat(dest, 0);
	return err;
}

int
pat_marshal(struct pattern *dest, uintptr_t root)
{
	int err = 0;

	err = vec_ctor(dest->prog);
	if (err) return ENOMEM;

	err = vec_concat_arr(&dest->prog, ((struct ins[]) {
		[0] = { .op = do_jump, .arg = {.f=2}, },
		[1] = { .op = do_clss, .arg = {.i=class_any}, },
		[2] = { .op = do_fork, .arg = {.f=-1}, },
	}));
	if (err) goto finally;

	err = comp_chld(&dest->prog, root);
	if (err) goto finally;

	err = vec_append(&dest->prog, ((struct ins[]) {
		{ .op = do_halt, .arg = {0}, },
	}));
	if (err) goto finally;

finally:
	if (err) vec_truncat(&dest->prog, 0);
	return err;
}
