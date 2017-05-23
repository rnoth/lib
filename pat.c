#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "pat.h"
#include "util.h"
#include "vec.h"

enum pat_char_class {
	class_any   = 0x0,
	class_dot   = 0x1,
	class_alpha = 0x2,
	class_upper = 0x3,
	class_lower = 0x4,
	class_digit = 0x5,
	class_space = 0x6,
};

enum pat_sym {
	sym_literal,
	sym_dot,
	sym_qmark,
	sym_star,
	sym_plus,
	sym_pipe,
	sym_carrot,
	sym_dollar,
	sym_lparen,
	sym_rparen,
};

enum type {
	type_null,
	type_root,
	type_cat,
	type_sub,
	type_opt,
	type_rep_null,
	type_rep,
	type_alt,
};

struct pos {
	char   const *v;
	size_t const  n;
	size_t        f;
};

struct node {
	uintptr_t chld[2];
	enum type type;
};

struct token {
	size_t       len;
	enum pat_sym type;
	wchar_t      wc;
};

struct state {
	struct pos   str[1];
	struct token tok[1];
	uintptr_t   *stk;
	struct node  root[1];
};

struct thread {
	struct patmatch *mat;
	struct ins      *ip;
};

struct context {
	int          (*cb)(char *, void *);
	void          *cbx;
	size_t         pos;
	struct thread *thr;
	struct thread  fin[1];
};

union arg {
	ptrdiff_t f;
	size_t    z;
	wchar_t   w;
	int       i;
};

struct ins {
        int      (*op)(struct context *, struct thread *, wchar_t const);
	union arg arg;
};

static int add_alter(struct state *);
static int add_literal(struct state *);
static int add_repetition(struct state *);
static int add_submatch(struct state *);
static int add_subtree(struct state *);

static int comp_cat(struct ins **, struct node *);
static int comp_init(struct ins **, struct node *);
static int comp_recurse(struct ins **, uintptr_t *);
static int comp_rep(struct ins **, struct node *);
static int comp_sub(struct ins **, struct node *);

static int  ctx_init(struct context *, struct pattern *);
static void ctx_fini(struct context *);

static int get_char(char *, void *);

static int ins_char(struct context *, struct thread *, wchar_t const);
static int ins_clss(struct context *, struct thread *, wchar_t const);
static int ins_fork(struct context *, struct thread *, wchar_t const);
static int ins_halt(struct context *, struct thread *, wchar_t const);
static int ins_jump(struct context *, struct thread *, wchar_t const);
static int ins_mark(struct context *, struct thread *, wchar_t const);
static int ins_save(struct context *, struct thread *, wchar_t const);

static uintptr_t mk_cat(uintptr_t, uintptr_t);

static void         nod_attach(uintptr_t, uintptr_t);
static struct node *nod_ctor(enum type, uintptr_t, uintptr_t);
static void         nod_dtor(struct node *);

static int       stk_pop(uintptr_t *, struct state *);
static int       stk_push(struct state *, uintptr_t);
static uintptr_t stk_reduce_concat(struct state *, size_t);
static int       stk_reduce_tree(struct state *);

static int             thr_cmp(struct thread *, struct thread *);
static struct thread * thr_ctor(struct ins *);
static void            thr_dtor(struct thread *);
static void            thr_finish(struct context *, size_t);
static int             thr_fork(struct thread *, struct thread *);
static int             thr_next(struct context *, size_t, wchar_t const);
static int             thr_start(struct context *, wchar_t const);
static void            thr_remove(struct context *, size_t);

static size_t pat_lex(struct state *);
static int    pat_marshal(struct pattern *, struct node *);
static int    pat_parse(struct node **, char const *);
static int    pat_parse_tree(struct state *);
static int    pat_do_match(struct context *, struct pattern *);
static int    pat_exec(struct context *);

static inline
bool        eol(struct pos const *p) { return p->f >= p->n; }

static inline
size_t      rem(struct pos const *p) { return p->n - p->f; }

static inline
char const *str(struct pos const *p) { return p->v + p->f; }

static inline bool nomatch(struct context *ctx) { return !ctx->fin->ip; }

static inline enum type    type(struct node *n) { return n ? n->type : 0x0; }
static inline struct node *to_node(uintptr_t u) { return (void *)(u & ~1); }
static inline struct ins  *to_leaf(uintptr_t u) { return (void *)u; }
static inline bool         is_leaf(uintptr_t u) { return u ? !(u & 1) : false; }
static inline bool         is_node(uintptr_t u) { return u ? u & 1 : false; }
static inline uintptr_t    tag_leaf(struct ins *i) { return (uintptr_t)i; }
static inline uintptr_t    tag_node(struct node *n) { return n ? (uintptr_t)n + 1 : 0x0; }

static int (* const pat_add[])(struct state *) = {
	[sym_pipe]    = add_alter,
	[sym_literal] = add_literal,
	[sym_qmark]   = add_repetition,
	[sym_star]    = add_repetition,
	[sym_plus]    = add_repetition,
	[sym_lparen]  = add_submatch,
	[sym_rparen]  = add_subtree,
};

static int (* const pat_comp[])(struct ins **, struct node *) = {
	[type_root]     = comp_init,
	[type_cat]      = comp_cat,
	[type_opt]      = comp_rep,
	[type_rep_null] = comp_rep,
	[type_rep]      = comp_rep,
	[type_sub]      = comp_sub,
};

int
add_alter(struct state *st)
{
	return -1;
}

int
add_literal(struct state *st)
{
	struct ins *p = calloc(1, sizeof *p);
	if (!p) return ENOMEM;

	memcpy(p, (struct ins[]){{
			.op = ins_char,
			.arg = { .w = st->tok->wc },
	}}, sizeof *p);

	return stk_push(st, tag_leaf(p));
}

int
add_repetition(struct state *st)
{
	enum type const tab[] = {
		[sym_qmark] = type_opt,
		[sym_plus]  = type_rep,
		[sym_star]  = type_rep_null,
	};
	struct node *new = 0x0;
	uintptr_t chld = 0x0;
	int err = 0;

	if (stk_pop(&chld, st)) return PAT_ERR_BADREP;

	new = nod_ctor(tab[st->tok->type], chld, 0x0);
	if (!new) goto nomem;

	err = stk_push(st, tag_node(new));
	if (err) goto fail;

	return 0;

nomem:
	err = ENOMEM;
fail:
	free(new);
	return err;
}

int
add_submatch(struct state *st)
{
	struct node *new = 0;

	new = nod_ctor(type_sub, 0x0, 0x0);
	if (!new) return ENOMEM;

	return stk_push(st, tag_node(new));
}

int
add_subtree(struct state *st)
{
	return stk_reduce_tree(st);
}

int
comp_cat(struct ins **dest, struct node *nod)
{
	int err = 0;
	struct node *chld;

	if (is_leaf(nod->chld[0])) {
		err = vec_append(dest, to_leaf(nod->chld[0]));
	} else if (is_node(nod->chld[0])) {
		chld = to_node(nod->chld[0]);
		err = pat_comp[chld->type](dest, chld);
	}

	if (err) return err;

	if (is_leaf(nod->chld[1])) {
		return vec_append(dest, to_leaf(nod->chld[1]));
	} else if (is_node(nod->chld[1])) {
		chld = to_node(nod->chld[1]);
		return pat_comp[chld->type](dest, chld);
	} else return 0;
}

int
comp_init(struct ins **dest, struct node *root)
{
	int err = 0;

	err = vec_concat_arr(dest, ((struct ins[]) {
		[0] = { .op = ins_jump, .arg = {.f=2}, },
		[1] = { .op = ins_clss, .arg = {.i=class_any}, },
		[2] = { .op = ins_fork, .arg = {.f=-1}, },
	}));
	if (err) goto finally;

	err = comp_recurse(dest, root->chld);
	if (err) goto finally;

finally:
	if (err) vec_truncat(dest, 0);
	return err;
}


int
comp_recurse(struct ins **dest, uintptr_t chld[static 2])
{
	int err = 0;
	uintptr_t u = 0x0;

	iterate (i, 2) {
		u = chld[i];

		if (!u) continue;
		else if (is_leaf(u)) err = vec_append(dest, to_leaf(u));
		else err = pat_comp[to_node(u)->type](dest, to_node(u));

		if (err) return err;
	}

	return 0;
}

int
comp_rep(struct ins **dest, struct node *nod)
{
	int err = 0;
	size_t beg = 0;
	size_t off = 0;

	off = vec_len(*dest);

	if (type(nod) != type_rep) {
		err = vec_append(dest, (struct ins[]) {{ .op = ins_fork }});
		if (err) return err;
	}

	beg = vec_len(*dest);

	err = comp_recurse(dest, nod->chld);
	if (err) return err;

	if (type(nod) != type_opt) {
		err = vec_append(dest, ((struct ins[]) {{
			.op = ins_fork,
			.arg = { .f = beg - vec_len(*dest) },
		}}));
		if (err) return err;
	}

	if (type(nod) != type_rep) {
		(*dest)[off].arg.f = vec_len(*dest) - off;
	}

	return 0;
}

int
comp_sub(struct ins **dest, struct node *root)
{
	int err = 0;

	err = vec_append(dest, ((struct ins[]) {
		{ .op = ins_mark, .arg = {0}, },
	}));
	if (err) goto finally;

	err = comp_recurse(dest, root->chld);
	if (err) goto finally;

	err = vec_concat_arr(dest, ((struct ins[]) {
		{ .op = ins_save, .arg = {0}, },
		{ .op = ins_halt, .arg = {0}, },
	}));
	if (err) goto finally;

finally:
	if (err) vec_truncat(dest, 0);
	return err;
}


int
ctx_init(struct context *ctx, struct pattern *pat)
{
	struct thread *th = 0x0;
	int err = 0;

	th = thr_ctor(pat->prog);
	if (!th) return ENOMEM;

	err = vec_ctor(ctx->thr);
	if (err) goto fail;

	err = vec_append(&ctx->thr, th);
	if (err) goto fail;

	return 0;

fail:
	thr_dtor(th);
	vec_free(ctx->thr);
	return err;
}

void
ctx_fini(struct context *ctx)
{
	struct thread *tmp;

	vec_foreach(tmp, ctx->thr) vec_free(tmp->mat);
	vec_free(ctx->thr);
	vec_free(ctx->fin->mat);
}

int
get_char(char *dst, void *x)
{
	union {
		void *p;
		struct pos *v;
	} str = { .p = x };
	struct pos *p = str.v;
	int len = 0;

	if (p->f > p->n) return 0;
	len = mblen(p->v + p->f, p->n - p->f);
	if (len <= 0) len = 1;

	memcpy(dst, p->v + p->f, len);

	p->f += len;

	return len;
}

int
ins_char(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t ind = th - ctx->thr;

	if (th->ip->arg.w != wc) thr_remove(ctx, th - ctx->thr), --ind;
	else ++th->ip;

	return thr_next(ctx, ind + 1, wc);
}

int
ins_clss(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t ind = th - ctx->thr;
	bool res;

	switch (th->ip->arg.i) {
	case class_any: res = true; break;
	case class_dot: res = wc != L'\n' && wc != L'\0'; break;
	case class_alpha: res = iswalpha(wc); break;
	case class_upper: res = iswupper(wc); break;
	case class_lower: res = iswlower(wc); break;
	case class_space: res = iswspace(wc); break;
	case class_digit: res = iswdigit(wc); break;
	default:
		assert(!"invalid argument to character class instruction");
	}

	if (!res) thr_remove(ctx, ind), --ind;
	else ++th->ip;

	return thr_next(ctx, ind + 1, wc);
}

int
ins_fork(struct context *ctx, struct thread *th, wchar_t const wc)
{
	struct thread new[1] = {{0}};
	size_t ind = th - ctx->thr;
	int err = 0;

	err = thr_fork(new, th);
	if (err) goto fail;

	new->ip += th->ip->arg.f;

	err = vec_insert(&ctx->thr, new, ind + 1);
	if (err) goto fail;

	th = ctx->thr + ind;

	++th->ip;
	return th->ip->op(ctx, th, wc);

fail:
	vec_free(new->mat);
	return err;
}

int
ins_halt(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t ind = th - ctx->thr;

	if (thr_cmp(ctx->fin, th) > 0) {
		thr_remove(ctx, ind);
		return thr_next(ctx, ind, wc);
	}

	thr_finish(ctx, ind);
	return thr_next(ctx, ind, wc);
}

int
ins_jump(struct context *ctx, struct thread *th, wchar_t const wc)
{
	th->ip += th->ip->arg.f;
	return th->ip->op(ctx, th, wc);
}

int
ins_mark(struct context *ctx, struct thread *th, wchar_t const wc)
{
	int err = 0;
	struct patmatch mat = {0};

	mat.off = ctx->pos;
	mat.ext = -1;

	err = vec_append(&th->mat, &mat);
	if (err) return err;

	++th->ip;
	return th->ip->op(ctx, th, wc);
}

int
ins_save(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t tag = th->ip->arg.z ;
	size_t beg = th->mat[tag].off ;
	size_t ext = ctx->pos - beg ;

	th->mat[tag].ext = ext ;

	th->ip++ ;

	return th->ip->op(ctx, th, wc);
}

uintptr_t
mk_cat(uintptr_t left, uintptr_t right)
{
	return tag_node(nod_ctor(type_cat, left, right));
}
 
void
nod_attach(uintptr_t par, uintptr_t chld)
{
	struct node *fin = to_node(par);

	while (fin->chld[1]) fin = to_node(fin->chld[1]);
	fin->chld[1] = chld;
}

struct node *
nod_ctor(enum type type, uintptr_t chld0, uintptr_t chld1)
{
	struct node *ret = calloc(1, sizeof *ret);

	if (!ret) return 0;

	ret->type = type;
	ret->chld[0] = chld0;
	ret->chld[1] = chld1;

	return ret;
}

void
nod_dtor(struct node *nod)
{
	uintptr_t u;
	if (!nod) return;
	arr_foreach(u, nod->chld) {
		if (is_leaf(u)) free(to_leaf(u));
		else nod_dtor(to_node(u));
	}

	free(nod);
}

int
stk_pop(uintptr_t *u, struct state *st)
{
	if (!vec_len(st->stk)) return -1;

	vec_pop(u, &st->stk);

	return 0;
}

int
stk_push(struct state *st, uintptr_t u)
{
	return vec_append(&st->stk, (uintptr_t[]){u});
}

uintptr_t
stk_reduce_concat(struct state *st, size_t off)
{
	uintptr_t new = 0x0;
	uintptr_t cat = 0x0;
	size_t i = 0;

	if (off == vec_len(st->stk)) return 0;

	for (i = off; i < vec_len(st->stk); ++i) {
		new = mk_cat(st->stk[i], 0x0);
		if (!new) goto nomem;
		if (cat) nod_attach(cat, new);
		else cat = new;
	}

	vec_truncat(&st->stk, off);

	return cat;

nomem:
	while (cat) {
		new = to_node(cat)->chld[1];
		free(to_node(cat));
		cat = new;
	}

	return 0x0;
}

int
stk_reduce_tree(struct state *st)
{
	struct node *nod = 0x0;
	size_t off = vec_len(st->stk);
	uintptr_t cat = 0x0;

	if (off == 0) return -1;

	while (off --> 0) {
		if (is_leaf(st->stk[off])) continue;

		nod = to_node(st->stk[off]);
		if (nod->type != type_sub) continue;

		if (nod->chld[0]) continue;

		break;
	}

	if (nod == 0x0) return -1;

	cat = stk_reduce_concat(st, off + 1);
	if (!cat) return ENOMEM;

	nod->chld[0] = cat;

	return 0;
}

int
thr_cmp(struct thread *lt, struct thread *rt)
{
	size_t min = 0;
	ptrdiff_t cmp = 0;
	size_t l = 0;
	size_t r = 0;

	if (!lt->mat && !rt->mat) return 0;
	if (!lt->mat) return -1;
	if (!rt->mat) return  1;

	min = umin(vec_len(lt->mat), vec_len(rt->mat));

	iterate(i, min) {
		l = lt->mat[i].off;
		r = rt->mat[i].off;
		cmp = ucmp(l, r);
		if (cmp) return -cmp;
		l = lt->mat[i].ext;
		r = rt->mat[i].ext;
		cmp = ucmp(l, r);
		if (cmp) return cmp;
	}

	return memcmp((size_t[]){vec_len(lt->mat)},
	              (size_t[]){vec_len(rt->mat)},
		      sizeof vec_len(0)); // ?
}

struct thread *
thr_ctor(struct ins *prog)
{
	struct thread *ret = 0x0;

	ret = calloc(1, sizeof *ret);
	if (!ret) return 0x0;

	ret->ip = prog;

	ret->mat = vec_new(struct thread);
	if (!ret->mat) {
		free(ret);
		return 0x0;
	}

	return ret;
}

void
thr_dtor(struct thread *th)
{
	if (!th) return;
	vec_free(th->mat);
	free(th);
}

void
thr_finish(struct context *ctx, size_t ind)
{
	if (ctx->fin->mat) vec_free(ctx->fin->mat);
	memcpy(ctx->fin, ctx->thr + ind, sizeof *ctx->fin);

	vec_delete(&ctx->thr, ind);
}

int
thr_fork(struct thread *dst, struct thread *src)
{
	dst->ip = src->ip;
	dst->mat = vec_clone(src->mat);

	return dst->mat ? 0 : ENOMEM;
}

int
thr_next(struct context *ctx, size_t ind, wchar_t wc)
{
	if (!vec_len(ctx->thr)) return -1;
	if (ind >= vec_len(ctx->thr)) return 0;
	else return ctx->thr[ind].ip->op(ctx, ctx->thr + ind, wc);
}

int
thr_start(struct context *ctx, wchar_t wc)
{
	return ctx->thr->ip->op(ctx, ctx->thr, wc);
}

void
thr_remove(struct context *ctx, size_t ind)
{
	vec_free(ctx->thr[ind].mat);
	vec_delete(&ctx->thr, ind);
}

int
pat_marshal(struct pattern *dest, struct node *root)
{
	return pat_comp[root->type](&dest->prog, root);
}

int
pat_do_match(struct context *ctx, struct pattern *pat)
{
	int err = pat_exec(ctx);
	if (err) return err;

	return vec_copy(&pat->mat, ctx->fin->mat);
}

int
pat_exec(struct context *ctx)
{
	char c[8];
	int err;
	int len;
	wchar_t wc = 0;
	wchar_t *wcs = 0;

	err = vec_ctor(wcs);
	if (err) return err;

	while (ctx->cb(c, ctx->cbx)) {

		len = mbtowc(&wc, c, 8);
		if (len <= 0) wc = *c; // XXX

		err = vec_concat(&wcs, &wc, len);
		if (err) break;

		err = thr_start(ctx, wc);
		if (err) break;

		ctx->pos += len;
	}
	
	vec_free(wcs);

	if (err > 0) return err;
	if (nomatch(ctx)) return -1;

	return 0;
}

size_t
pat_lex(struct state *st)
{
	int ret = 0;
	struct token *tok = st->tok;
	struct pos *pos = st->str;

	ret = tok->len = 1;
	switch (*str(pos)) {
	case '.':
		tok->type = sym_dot;
		break;

	case '?':
		tok->type = sym_qmark;
		break;

	case '*':
		tok->type = sym_star;
		break;

	case '+':
		tok->type = sym_plus;
		break;

	case '(':
		tok->type = sym_lparen;
		break;

	case ')':
		tok->type = sym_rparen;
		break;

	case '|':
		tok->type = sym_pipe;
		break;

	case '^':
		tok->type = sym_carrot;
		break;

	case '$':
		tok->type = sym_dollar;
		break;

	case '\\':
		goto esc;

	default:
		tok->type = sym_literal;
		ret = tok->len = mbtowc(&tok->wc, str(pos), rem(pos));
		if (ret == -1) {
			ret = 1;
			tok->wc = pos->v[0];
		} else if (ret == 0) ret = 1;
	}

	return ret;

esc:
	switch (pos->v[pos->f + 1]) {
	default:
		tok->type = sym_literal;
		ret += tok->len = mbtowc(&tok->wc, str(pos), rem(pos));
		if (ret == -1) {
			ret = 1;
			tok->wc = pos->v[0];
		} else if (ret == 0) ret = 1;
	}

	return ret;
}

int
pat_parse(struct node **rootp, char const *src)
{
	struct state st[1] = {{
		.str = {{ .v = src, .n = strlen(src), }}
	}};
	int err = 0;

	*rootp = nod_ctor(type_root, 0x0, 0x0);
	if (!*rootp) return ENOMEM;

	err = vec_ctor(st->stk);
	if (err) goto finally;

	err = pat_parse_tree(st);
	if (err) goto finally;

	rootp[0]->chld[0] = st->stk[0];

finally:
	vec_free(st->stk);

	return err;
}

int
pat_parse_tree(struct state *st)
{
	struct node *nod = 0x0;
	int err = 0;

	nod = nod_ctor(type_sub, 0x0, 0x0);
	if (!nod) return ENOMEM;

	err = vec_append(&st->stk, (uintptr_t[]){tag_node(nod)});
	if (err) { free(nod); return err; }

	while (!eol(st->str)) {
		st->str->f += pat_lex(st);
		err = pat_add[st->tok->type](st);
		if (err) return err;
	}

	err = stk_reduce_tree(st);
	if (err) return err;

	st->root->chld[0] = st->stk[0];
	return 0;
}

int
pat_compile(struct pattern *dest, char const *src)
{
	int err = 0;
	struct node *root = 0x0;

	if (!dest) return EFAULT;
	if (!src) return EFAULT;

	err = vec_ctor(dest->prog);
	if (err) goto finally;

	err = vec_ctor(dest->mat);
	if (err) goto finally;

	err = pat_parse(&root, src);
	if (err) goto finally;

	err = pat_marshal(dest, root);
	if (err) goto finally;

finally:
	nod_dtor(root);
	if (err) vec_free(dest->prog);
	if (err) vec_free(dest->mat);
	return err;
}

void
pat_free(struct pattern *pat)
{
	vec_free(pat->prog);
	vec_free(pat->mat);
}

int
pat_match(struct pattern *pat, char const *str)
{
	if (!str) return EFAULT;
	return pat_match_callback(pat, get_char, (struct pos[]){
			{ .n = strlen(str) + 1, .v = str },
	});
}

int
pat_match_callback(struct pattern *pat, int (*cb)(char *, void *), void *cbx)
{
	struct context ctx[1] = {{ .cb = cb, .cbx = cbx }};
	int err = 0;

	if (!pat) return EFAULT;
	if (!cb)  return EFAULT;

	err = ctx_init(ctx, pat);
	if (err) goto finally;

	err = pat_do_match(ctx, pat);
	if (err) goto finally;

finally:
	ctx_fini(ctx);
	return err;
}
