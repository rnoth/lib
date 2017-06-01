#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "pat.h"
#include "util.h"
#include "vec.h"

#define tag_node(n) ((umin((uintptr_t)(n),-1)+1))
#define tag_leaf(l) ((uintptr_t)l)

#define to_node(n) ((struct node *)(n - 1))
#define to_leaf(l) ((char *)l)

enum class {
	class_none  = 0x00,
	class_any   = 0x01,
	class_dot   = 0x02,
	class_alpha = 0x04,
	class_upper = 0x08,
	class_lower = 0x10,
	class_digit = 0x20,
	class_space = 0x40,
};

enum token {
	tok_bol,
	tok_carrot,
	tok_dollar,
	tok_dot,
	tok_eol,
	tok_liter,
	tok_lparen,
	tok_pipe,
	tok_plus,
	tok_qmark,
	tok_rparen,
	tok_star,
};

enum type {
	type_null,
	type_alt,
	type_cat,
	type_class,
	type_leaf,
	type_opt,
	type_rep,
	type_rep_null,
	type_sub,
};

struct pos {
	char   const *v;
	size_t const  n;
	size_t        f;
};

struct node {
	uintptr_t chld[2];
	size_t    len;
	enum type type;
};

struct state {
	struct pos   src[1];
	enum token   tok;
	uint8_t     *stk;
	uint8_t     *ir;
	uint8_t     *aux;
	uintptr_t   *trv;
};

struct thread {
	struct ins      *ip;
	struct patmatch *mat;
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
	int       i;
	char      b;
	size_t    z;
};

struct ins {
        int      (*op)(struct context *, struct thread *, char const);
	union arg arg;
};

static inline
void       step(struct pos *);
static inline
bool        eol(struct pos const *);
static inline
char const *str(struct pos const *);

static inline
bool nomatch(struct context *);

static inline bool is_leaf(uintptr_t);
static inline bool is_node(uintptr_t);

static int comp_alt(struct ins **, struct node *);
static int comp_cat(struct ins **, struct node *);
static int comp_chld(struct ins **, uintptr_t);
//static int comp_class(struct ins **, struct node *);
static int comp_rep(struct ins **, struct node *);
static int comp_sub(struct ins **, struct node *);

static void ctx_fini(struct context *);
static int  ctx_init(struct context *, struct pattern *);

static int do_char(struct context *, struct thread *, char const);
static int do_clss(struct context *, struct thread *, char const);
static int do_fork(struct context *, struct thread *, char const);
static int do_halt(struct context *, struct thread *, char const);
static int do_jump(struct context *, struct thread *, char const);
static int do_mark(struct context *, struct thread *, char const);
static int do_save(struct context *, struct thread *, char const);

static int get_char(char *, void *);

static int grow_cat(struct state *);
static int grow_char(struct state *);
static int grow_close(struct state *);
static int grow_open(struct state *);

static bool is_open(uintptr_t);

static int shift_escape(struct state *);
static int shift_liter(struct state *);
static int shift_token(struct state *);

static uintptr_t mk_cat(uintptr_t, uintptr_t);

static uintptr_t   nod_attach(uintptr_t, uintptr_t);
static void       nod_dtor(uintptr_t);
static size_t     nod_len(uintptr_t);
static enum type  nod_type(uintptr_t);

static int  st_init(struct state *, char const *);
static void st_fini(struct state *);

static int  thr_cmp(struct thread *, struct thread *);
static int  thr_init(struct thread *, struct ins *);
static void thr_finish(struct context *, size_t);
static int  thr_fork(struct thread *, struct thread *);
static int  thr_next(struct context *, size_t, wchar_t const);
static void thr_prune(struct context *, size_t);
static int  thr_start(struct context *, wchar_t const);
static void thr_remove(struct context *, size_t);

static int pat_exec(struct context *);
static int pat_grow(struct state *);
static int pat_shunt(struct state *);
static int pat_marshal(struct pattern *, uintptr_t);
static int pat_parse(uintptr_t *, struct state *st);
static int pat_process(struct state *st);
static int pat_match(struct context *, struct pattern *);

void       step(struct pos *p) { ++p->f; }
bool        eol(struct pos const *p) { return p->f >= p->n; }
bool        bol(struct pos const *p) { return p->f == 0; }
char const *str(struct pos const *p) { return p->v + p->f; }

bool nomatch(struct context *ctx) { return !ctx->fin->ip; }

bool is_leaf(uintptr_t u) { return u ? !(u & 1) : false; }
bool is_node(uintptr_t u) { return u ? u & 1 : false; }

static int (* const tab_shift[])(struct state *) = {
	['\\'] = shift_escape,
	['*']  = shift_token,
	['?']  = shift_token,
	['+']  = shift_token,
	['(']  = shift_token,
	[')']  = shift_token,
	['.']  = shift_token,
	['|']  = shift_token,
	[255]  = 0,
};

static int (* const tab_shunt[])(struct state *) = {
	[255] = 0,
};

static int (* const tab_grow[])(struct state *) = {
	['_'] = grow_cat,
	['('] = grow_open,
	[')'] = grow_close,
	[255] = 0,
};

static int (* const pat_comp[])(struct ins **, struct node *) = {
	[type_alt]      = comp_alt,
	[type_cat]      = comp_cat,
	[type_opt]      = comp_rep,
	[type_rep_null] = comp_rep,
	[type_rep]      = comp_rep,
	[type_sub]      = comp_sub,
	//[type_class]    = comp_class,
};

static uint8_t const pat_prec[] = {
	[type_cat]      =  1,
	[type_alt]      =  3,
	[type_opt]      =  2,
	[type_rep_null] =  2,
	[type_rep]      =  2,
	[type_sub]      = -1,
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
		return vec_append(dest, ((struct ins[]) {{
			.op = do_char,
			.arg = {.b=*to_leaf(n)},
		}}));
	}
	struct node *nod = to_node(n);
	return pat_comp[nod->type](dest, nod);
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

void
ctx_fini(struct context *ctx)
{
	struct thread *th;

	vec_foreach(th, ctx->thr) vec_free(th->mat);
	vec_free(ctx->thr);
	vec_free(ctx->fin->mat);
}

int
ctx_init(struct context *ctx, struct pattern *pat)
{
	struct thread th[1] = {{0}};
	int err = 0;

	err = thr_init(th, pat->prog);
	if (err) return err;

	err = vec_ctor(ctx->thr);
	if (err) goto fail;

	err = vec_append(&ctx->thr, th);
	if (err) goto fail;

	return 0;

fail:
	vec_free(th->mat);
	vec_free(ctx->thr);
	return err;
}

int
do_char(struct context *ctx, struct thread *th, char const ch)
{
	size_t ind = th - ctx->thr;

	if (th->ip->arg.b != ch) thr_remove(ctx, th - ctx->thr), --ind;
	else ++th->ip;

	return thr_next(ctx, ind + 1, ch);
}

int
do_clss(struct context *ctx, struct thread *th, char const ch)
{
	size_t ind = th - ctx->thr;
	bool res;

	switch (th->ip->arg.i) {
	case class_any: res = true; break;
	case class_dot: res = ch != L'\n' && ch != L'\0'; break;
	case class_alpha: res = isalpha(ch); break;
	case class_upper: res = isupper(ch); break;
	case class_lower: res = islower(ch); break;
	case class_space: res = isspace(ch); break;
	case class_digit: res = isdigit(ch); break;
	default:
		return -1; // XXX
	}

	if (!res) thr_remove(ctx, ind), --ind;
	else ++th->ip;

	return thr_next(ctx, ind + 1, ch);
}

int
do_fork(struct context *ctx, struct thread *th, char const ch)
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
	return th->ip->op(ctx, th, ch);

fail:
	vec_free(new->mat);
	return err;
}

int
do_halt(struct context *ctx, struct thread *th, char const ch)
{
	struct patmatch term[] = {{ .off = -1, .ext = -1 }};
	size_t ind = th - ctx->thr;
	int err;

	if (thr_cmp(ctx->fin, th) > 0) {
		thr_remove(ctx, ind);
		return thr_next(ctx, ind, ch);
	}

	err = vec_append(&th->mat, term);
	if (err) return err;

	thr_finish(ctx, ind);
	return thr_next(ctx, ind, ch);
}

int
do_jump(struct context *ctx, struct thread *th, char const ch)
{
	th->ip += th->ip->arg.f;
	return th->ip->op(ctx, th, ch);
}

int
do_mark(struct context *ctx, struct thread *th, char const ch)
{
	int err = 0;
	struct patmatch mat = {0};

	mat.off = ctx->pos;
	mat.ext = -1;

	err = vec_append(&th->mat, &mat);
	if (err) return err;

	++th->ip;
	return th->ip->op(ctx, th, ch);
}

int
do_save(struct context *ctx, struct thread *th, char const ch)
{
	size_t off = vec_len(th->mat);

	while (off --> 0) if (th->mat[off].ext == (size_t)-1) break;

	th->mat[off].ext = ctx->pos - th->mat[off].off;

	th->ip++;
	return th->ip->op(ctx, th, ch);
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
grow_cat(struct state *st)
{
	struct node *cat;
	uintptr_t lef;
	uintptr_t rit;

	cat = calloc(1, sizeof *cat);
	if (!cat) return ENOMEM;

	vec_get(&rit, st->trv);
	vec_get(&lef, st->trv);

	cat->type = type_cat;
	cat->chld[0] = lef;
	cat->chld[1] = rit;
	cat->len = nod_len(lef) + nod_len(rit);

	vec_append(&st->trv, (uintptr_t[]){tag_node(cat)});

	++st->ir;
	return pat_grow(st);
}

int
grow_char(struct state *st)
{
	uintptr_t prev;
	uint8_t *res;

	prev = st->trv[vec_len(st->trv)-1];

	if (!is_leaf(prev)) {
		if (vec_len(st->aux)) vec_put(st->aux, (uint8_t[]){0});
		res = st->aux + vec_len(st->aux);
		vec_put(st->aux, st->ir);
		vec_put(st->trv, (uintptr_t[]){tag_leaf(res)});
	} else vec_put(st->aux, st->ir);

	++st->ir;

	return pat_grow(st);
}

int
grow_close(struct state *st)
{
	uintptr_t lef;
	uintptr_t rit;
	uintptr_t res;

again:
	vec_get(&rit, st->trv);
	if (is_open(rit)) {
		vec_put(st->trv, &rit);
		return pat_grow(st);
	}

	vec_get(&lef, st->trv);
	res = nod_attach(lef, rit);

	vec_put(st->trv, &res);

	if (nod_type(res) != type_sub) goto again;

	++st->ir;
	return pat_grow(st);
}

int
grow_open(struct state *st)
{
	struct node *nod;

	nod = calloc(1, sizeof *nod);
	if (!nod) return ENOMEM;

	nod->type = type_sub;
	vec_append(&st->trv, (uintptr_t[]){tag_node(nod)});

	++st->ir;
	return pat_grow(st);
}

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

int
shift_escape(struct state *st)
{
	step(st->src);
	vec_put(st->stk, (char[]){'\\'});
	vec_put(st->stk, str(st->src));
	step(st->src);
	return pat_shunt(st);
}

int
shift_liter(struct state *st)
{
	uint8_t ch; 

	memcpy(&ch, str(st->src), 1);
	if (tab_shift[ch]) {
		vec_put(st->stk, (char[]){'\\'});
	}

	vec_put(st->stk, str(st->src));

	step(st->src);
	return pat_shunt(st);
}

int
shift_token(struct state *st)
{
	vec_put(st->aux, str(st->src));
	step(st->src);

	return pat_shunt(st);
}

uintptr_t
mk_cat(uintptr_t lef, uintptr_t rig)
{
	struct node *cat = 0;

	cat = calloc(1, sizeof *cat);
	if (cat) {
		cat->type = type_cat;
		cat->chld[0] = lef;
		cat->chld[1] = rig;
	}

	return tag_node(cat);
}

uintptr_t
nod_attach(uintptr_t lef, uintptr_t rit)
{
	uintptr_t cat;
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

void
st_fini(struct state *st)
{
	vec_free(st->stk);
	vec_free(st->aux);
	vec_free(st->trv);
}

int
st_init(struct state *st, char const *src)
{
	memcpy(st->src, (struct pos[]){{
		.v = src,
		.n = strlen(src)
	}}, sizeof *st->src);
	st->stk = vec_alloc(uint8_t, st->src->n * 2 + 2);
	st->aux = vec_alloc(uint8_t, st->src->n * 2);
	st->trv = vec_alloc(uintptr_t, st->src->n);

	if (!st->aux || !st->stk || !st->trv) {
		vec_free(st->stk);
		vec_free(st->aux);
		vec_free(st->trv);

		return ENOMEM;
	}

	return 0;
}

int
thr_cmp(struct thread *lt, struct thread *rt)
{
	size_t min = 0;
	ptrdiff_t cmp = 0;

	if (!lt->mat && !rt->mat) return 0;
	if (!lt->mat) return -1;
	if (!rt->mat) return  1;

	min = umin(vec_len(lt->mat), vec_len(rt->mat));

	iterate(i, min) {
		cmp = ucmp(lt->mat[i].off, rt->mat[i].off);
		if (cmp) return -cmp;

		cmp = ucmp(lt->mat[i].ext, rt->mat[i].ext);
		if (cmp) return cmp;
	}

	return 0;
}

int
thr_init(struct thread *th, struct ins *prog)
{
	th->ip = prog;

	th->mat = vec_new(struct thread);
	if (!th->mat) return ENOMEM;;

	return 0;
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
	if (!vec_len(ctx->thr)) return 0;
	if (ind >= vec_len(ctx->thr)) return 0;

	if (ctx->fin->mat) thr_prune(ctx, ind);

	if (ind >= vec_len(ctx->thr)) return 0;
	return ctx->thr[ind].ip->op(ctx, ctx->thr + ind, wc);
}

void
thr_prune(struct context *ctx, size_t ind)
{
	size_t i;

	for (i = ind; i < vec_len(ctx->thr);) {
		if (thr_cmp(ctx->fin, ctx->thr + i) > 0) thr_remove(ctx, i);
		else return;
	}
}

int
thr_start(struct context *ctx, wchar_t wc)
{
	return thr_next(ctx, 0, wc);
}

void
thr_remove(struct context *ctx, size_t ind)
{
	vec_free(ctx->thr[ind].mat);
	vec_delete(&ctx->thr, ind);
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

int
pat_match(struct context *ctx, struct pattern *pat)
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

int
pat_grow(struct state *st)
{
	uint8_t ch;
	int (*fn)();
	size_t off = st->ir - st->stk;

	if (off >= vec_len(st->stk)) return 0;

	memcpy(&ch, st->ir, 1);

	if (!ch) return 0;

	fn = tab_grow[ch];
	if (fn) return fn(st);
	else return grow_char(st);
}

int
pat_flush(struct state *st)
{
	st->ir = st->stk;
	vec_zero(st->aux);
	return 0;
}

int
pat_shunt(struct state *st)
{
	int (*fn)();
	uint8_t ch;

	if (eol(st->src)) return pat_flush(st);

	memcpy(&ch, str(st->src), 1);

	fn = tab_shift[ch];
	if (fn) return fn(st);
	else return shift_liter(st);
}

int
pat_parse(uintptr_t *dst, struct state *st)
{
	int err;

	err = pat_process(st);
	if (err) return err;

	err = pat_grow(st);
	if (err) return err;

	*dst = st->trv[0];

	return 0;
}

int
pat_process(struct state *st)
{
	int err;

	vec_put(st->stk, (char[]){'('});
	err = pat_shunt(st);
	if (err) return err;
	vec_put(st->stk, (char[]){')'});

	return 0;
}

int
pat_compile(struct pattern *dst, char const *src)
{
	struct state st[1] = {{0}};
	uintptr_t root;
	int err = 0;

	if (!dst) return EFAULT;
	if (!src) return EFAULT;

	err = st_init(st, src);
	if (err) goto finally;

	err = pat_parse(&root, st);
	if (err) goto finally;

	err = vec_ctor(dst->prog);
	if (err) goto finally;

	err = pat_marshal(dst, root);
	if (err) goto finally;

finally:
	st_fini(st);
	nod_dtor(root);
	return err;
}

void
pat_free(struct pattern *pat)
{
	vec_free(pat->prog);
	vec_free(pat->mat);
}

int
pat_execute(struct pattern *pat, char const *str)
{
	if (!str) return EFAULT;
	return pat_execute_callback(pat, get_char, (struct pos[]){
			{ .n = strlen(str) + 1, .v = str },
	});
}

int
pat_execute_callback(struct pattern *pat, int (*cb)(char *, void *), void *cbx)
{
	struct context ctx[1] = {{ .cb = cb, .cbx = cbx }};
	int err = 0;

	if (!pat) return EFAULT;
	if (!cb)  return EFAULT;

	err = ctx_init(ctx, pat);
	if (err) goto finally;

	err = vec_ctor(pat->mat);
	if (err) goto finally;

	err = pat_match(ctx, pat);
	if (err) goto finally;

finally:
	ctx_fini(ctx);
	return err;
}
