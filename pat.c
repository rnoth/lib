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
	struct ins **buf;
	uintptr_t  **stk;
	struct node *cur;
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
	struct thread *fin;
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
static int comp_recurse(struct ins **, uintptr_t *);
static int comp_rep(struct ins **, struct node *);
static int comp_root(struct ins **, struct node *);

static uintptr_t buf_concat(struct ins **buf);

static int  ctx_init(struct context *);
static void ctx_fini(struct context *);

static int get_char(char *, void *);

static int ins_char(struct context *, struct thread *, wchar_t const);
static int ins_clss(struct context *, struct thread *, wchar_t const);
static int ins_fork(struct context *, struct thread *, wchar_t const);
static int ins_halt(struct context *, struct thread *, wchar_t const);
static int ins_jump(struct context *, struct thread *, wchar_t const);
static int ins_mark(struct context *, struct thread *, wchar_t const);
static int ins_save(struct context *, struct thread *, wchar_t const);

static struct node *mk_append(struct node *, struct node *);
static struct node *mk_cat(struct node *, struct node *);

static struct node *nod_ctor(enum type, uintptr_t, uintptr_t);
static void         nod_dtor(struct node *);

static int       st_addtree(struct state *);
static uintptr_t st_concat(struct state *);
static int       st_flush(struct state *);
static int       st_init(struct state *);
static int       st_push(struct state *, struct node *);

static int  thr_next(struct context *, size_t, wchar_t const);
static int  thr_finish(struct context *, size_t);
static int  thr_fork(struct thread *, struct thread *);
static int  thr_start(struct context *, wchar_t const);
static void thr_remove(struct context *, size_t);

static size_t pat_lex(struct state *);
static int    pat_marshal(struct pattern *, struct node *);
static int    pat_parse(struct node **, char const *);
static int    pat_parse_tree(struct state *);
static int    pat_do_match(struct pattern *, struct context *);
static int    pat_exec(struct context *);

static inline
bool        eol(struct pos const *p) { return p->f >= p->n; }

static inline
size_t      rem(struct pos const *p) { return p->n - p->f; }

static inline
char const *str(struct pos const *p) { return p->v + p->f; }

static inline enum type    type(struct node *n) { return n ? n->type : 0x0; }
static inline struct node *to_node(uintptr_t u) { return (void *)(u & ~1); }
static inline struct ins  *to_leaf(uintptr_t u) { return (void *)u; }
static inline bool         is_leaf(uintptr_t u) { return u ? !(u & 1) : false; }
static inline bool         is_node(uintptr_t u) { return u ? u & 1 : false; }
static inline uintptr_t    tag_leaf(struct ins *i) { return (uintptr_t)i; }
static inline uintptr_t    tag_node(struct node *n) { return n ? (uintptr_t)n + 1 : 0x0; }

static inline
uintptr_t **stk_top(struct state *st)
{ return vec_len(st->stk) ? st->stk + vec_len(st->stk) - 1 : 0x0; }

static inline
struct node *stk_root(struct state *st)
{ return vec_len(st->stk) ? to_node(st->stk[vec_len(st->stk)-1][0]) : 0x0; }

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
	[type_root]     = comp_root,
	[type_cat]      = comp_cat,
	[type_opt]      = comp_rep,
	[type_rep_null] = comp_rep,
	[type_rep]      = comp_rep,
};

int
add_alter(struct state *st)
{
	struct node *nod = 0x0;
	uintptr_t chld = 0x0;
	int err = 0;

	if (type((stk_root(st))) == type_alt) return -1;

	st_flush(st);

	chld = st_concat(st);
	if (!chld) return ENOMEM;
	vec_truncat(&st->stk, 1);

	nod = nod_ctor(type_alt, 0x0, chld);
	if (!nod) return ENOMEM; // XXX

	err = st_push(st, nod);
	if (err) return err; // XXX

	return 0;
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

	return vec_append(&st->buf, &p);
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
	void *tmp = 0x0;
	int err = 0;

	if (vec_len(st->buf)) {
		vec_pop(&tmp, &st->buf);
		chld = tag_leaf(tmp);

	} else if (vec_len(st->stk)) {
		vec_pop(&tmp, stk_top(st));
		chld = tag_node(tmp);

	} else return PAT_ERR_BADREP;

	new = nod_ctor(tab[st->tok->type], chld, 0x0);
	if (!new) goto nomem;

	err = st_flush(st);
	if (err) goto fail;

	err = st_push(st, new);
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
	int err = 0;
	struct node *new = 0;

	new = nod_ctor(type_sub, 0x0, 0x0);
	if (!new) return ENOMEM;

	err = st_push(st, new);
	if (err) return err;

	return 0;
}

int
add_subtree(struct state *st)
{
	if (vec_len(st->stk) < 2) return PAT_ERR_BADPAREN;
	return st_addtree(st);
}

uintptr_t
buf_concat(struct ins **buf)
{
	struct node *nod = 0x0;
	uintptr_t ret = 0x0;
	size_t len = vec_len(buf);

	while (len --> 0) {
		nod = nod_ctor(type_cat, tag_leaf(buf[len]), ret);
		ret = tag_node(nod);
	}

	return ret;
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
comp_root(struct ins **dest, struct node *root)
{
	int err = 0;

	err = vec_concat_arr(dest, ((struct ins[]) {
		[0] = { .op = ins_jump, .arg = {.f=2}, },
		[1] = { .op = ins_clss, .arg = {.i=class_any}, },
		[2] = { .op = ins_fork, .arg = {.f=-1}, },
		[3] = { .op = ins_mark, .arg = {0}, },
	}));
	if (err) goto finally;

	err = comp_recurse(dest, root->chld);
	if (err) goto finally;

	err = vec_concat_arr(dest, ((struct ins[]) {
		[0] = { .op = ins_save, .arg = {0}, },
		[1] = { .op = ins_halt, .arg = {0}, },
	}));
	if (err) goto finally;

finally:
	if (err) vec_truncat(dest, 0);
	return err;
}

int
ctx_init(struct context *ctx)
{
	int err = 0;

	err = vec_ctor(ctx->thr);
	if (err) goto nomem;

	err = vec_ctor(ctx->fin);
	if (err) goto nomem;

	return 0;
nomem:
	vec_free(ctx->thr);
	vec_free(ctx->fin);
	return err;

}

void
ctx_fini(struct context *ctx)
{
	struct thread *tmp;

	vec_foreach(tmp, ctx->thr) vec_free(tmp->mat);
	vec_foreach(tmp, ctx->fin) vec_free(tmp->mat);
	vec_free(ctx->thr);
	vec_free(ctx->fin);
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
	int err = 0;
	size_t ind = th - ctx->thr;

	err = thr_finish(ctx, ind);
	if (err) return err;

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

struct node *
mk_append(struct node *dest, struct node *src)
{
	struct node *fin = dest;

	while (fin->chld[1]) fin = to_node(fin->chld[1]);

	fin->chld[1] = tag_node(src);

	return dest;
}

struct node *
mk_cat(struct node *dest, struct node *src)
{
	return nod_ctor(type_cat, tag_node(dest), tag_node(src));
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

uintptr_t
st_concat(struct state *st)
{
	uintptr_t *stk = *stk_top(st);
	struct node *tmp = 0x0;
	struct node *cat = 0x0;
	struct node *nod = 0x0;
	size_t len = vec_len(*stk_top(st));

	while (len --> 1) {
		nod = to_node(stk[len]);

		if (nod->type == type_cat) tmp = mk_append(nod, cat);
		else if (cat) tmp = mk_cat(nod, cat);
		else { cat = nod; continue; }

		if (!tmp) goto fail;
		cat = tmp;
	}


	return tag_node(cat);
fail:
	nod_dtor(cat);
	return ENOMEM;
}

int
st_flush(struct state *st)
{
	uintptr_t u = buf_concat(st->buf);
	vec_truncat(&st->buf, 0);
	if (u) return vec_append(stk_top(st), (uintptr_t[]){u});
	else return 0;
}

int
st_addtree(struct state *st)
{
	struct node *par = 0x0;
	int err = 0;

	if (!vec_len(st->stk)) return -1;

	err = st_flush(st);
	if (err) return err;

	par = to_node(*stk_top(st)[0]);
	par->chld[0] = st_concat(st);
	if (!par->chld[0]) return ENOMEM;

	vec_free(*stk_top(st));
	vec_delete(&st->stk, stk_top(st) - st->stk);

	if (vec_len(st->stk)) return st_push(st, par);

	st->cur = par;
	return 0;
}

int
st_init(struct state *st)
{
	struct node *nod = 0x0;
	uintptr_t *u = 0x0;
	int err = 0;

	nod = nod_ctor(type_root, 0x0, 0x0);
	if (!nod) return ENOMEM;

	err = vec_ctor(st->stk);
	if (err) goto fail;

	err = vec_ctor(u);
	if (err) goto fail;

	err = vec_append(&st->stk, &u);
	if (err) goto fail;

	err = vec_append(st->stk, (uintptr_t[]){ tag_node(nod) });
	if (err) goto fail;

	return 0;

fail:
	if (st->stk) vec_free(st->stk[0]);
	vec_free(st->stk);
	vec_free(st->buf);

	return err ? err : ENOMEM;
}

int
st_push(struct state *st, struct node *nod)
{
	return vec_append(stk_top(st), (uintptr_t[]){ tag_node(nod) });
}

int
thr_finish(struct context *ctx, size_t ind)
{
	int err = 0;

	err = vec_append(&ctx->fin, ctx->thr + ind);
	if (err) return err;
	vec_delete(&ctx->thr, ind);

	return 0;
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
pat_do_match(struct pattern *pat, struct context *ctx)
{
	int err = 0;
	size_t max = 0;
	struct thread *it;

	err = pat_exec(ctx);
	if (err) return err;

	vec_foreach (it, ctx->fin) {
		if (it->mat[0].ext > ctx->fin[max].mat[0].ext
		&& it->mat[0].off <= ctx->fin[max].mat[0].off) {
			max = it - ctx->fin;
		}
	}

	return vec_copy(&pat->mat, ctx->fin[max].mat);
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
	if (!vec_len(ctx->fin)) return -1;
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
	int err = 0;
	struct state st[1] = {{
		.str = {{ .v = src, .n = strlen(src), }}
	}};

	err = st_init(st);
	if (err) goto finally;

	err = vec_ctor(st->buf);
	if (err) goto finally;

	err = pat_parse_tree(st);
	if (err) goto finally;

finally:
	if (err) nod_dtor(st->cur);
	vec_free(st->buf);

	*rootp = st->cur;

	return err;
}

int
pat_parse_tree(struct state *st)
{
	int err = 0;

	while (!eol(st->str)) {
		st->str->f += pat_lex(st);
		err = pat_add[st->tok->type](st);
		if (err) return err;
	}

	do err = st_addtree(st); while (!err);
	if (err > 0) return err;
	else return 0;

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
	struct context ctx[1] = {{.cb = cb, .cbx = cbx}};
	struct thread th[1] = {{ .ip = pat->prog }};
	int err = 0;

	if (!pat) return EFAULT;
	if (!cb) return EFAULT;

	err = ctx_init(ctx);
	if (err) goto finally;

	err = vec_ctor(th->mat);
	if (err) goto finally;

	err = vec_append(&ctx->thr, th);
	if (err) goto finally;

	err = pat_do_match(pat, ctx);
	if (err) goto finally;

finally:
	ctx_fini(ctx);
	return err;
}
