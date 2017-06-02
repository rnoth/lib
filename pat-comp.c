#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <util.h>
#include <vec.h>
#include <pat.ih>
#include <pat.h>

static int (* const pat_comp[])(struct ins **, struct node *) = {
	[type_alt]      = comp_alt,
	[type_cat]      = comp_cat,
	[type_opt]      = comp_rep,
	[type_rep_null] = comp_rep,
	[type_rep]      = comp_rep,
	[type_sub]      = comp_sub,
	//[type_class]    = comp_class,
};

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
	if (is_leaf(n)) {
		return comp_leaf(dest, n);
	}
	struct node *nod = to_node(n);
	return pat_comp[nod->type](dest, nod);
}

int
comp_leaf(struct ins **dest, uintptr_t lea)
{
	char *s = to_leaf(lea);
	int err;
	do {
		err = vec_append(dest, ((struct ins[]) {{
			.op = do_char,
			.arg = {.b = *s},
		}}));
		if (err) return err;
	} while (*++s);

	return 0;
}

#if 0
int
comp_class(struct ins **dest, struct node *nod)
{
	int err;
	union { void *v; enum class *c; } p;

	p.v = to_aux(nod->chld[0]);

	err = vec_append(dest, ((struct ins[]) {
		{ .op = do_clss, .arg = { .i = *p.c } }
	}));
	if (err) return err;

	return comp_chld(dest, nod->chld[1]);
}
#endif

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
