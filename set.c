#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "set.h"
#include "util.h"
#include "vec.h"

#define isleaf(nod) (!len((nod)->chld))

typedef Vector(uint8_t) Elem;
typedef struct Node Node;
typedef struct Reply Reply;

struct Node {
	Vector(uint8_t) *edge;
	Vector(Node *)  *chld;
};

struct Reply {
	size_t off;
	size_t ext;
	Node *nod;
};

struct Set {
	Node *root;
};

static void  free_node(Node *);
static Node *alloc_node(void);

static int	align	 (Node *);
static int 	attach	 (Node *, Node const *);
static void	delete	 (Node *, Node const *);
static int	insert	 (Node *, Elem const *);
static Node **	locate	 (Node *, Elem const *);
static size_t	match	 (Node const *, Elem const *);
static void *	marshal	 (Node const *, Elem const *);
static Node *	search	 (Node const *, Elem const *);
static int	split	 (Node *, size_t);
static Reply *	traverse (Node *, Elem const *);

static Elem *make_elem(void const *, size_t);

int
align(Node *nod)
{
	Node *chld;
	if (len(nod->chld) != 1) return 0;

	chld = *arr(nod->chld);
	if (vec_join(nod->edge, chld->edge))
		return ENOMEM;

	vec_free(chld->edge);
	vec_free(nod->chld);
	nod->chld = chld->chld;
	free(chld);
	return 0;
}

Node *
alloc_node(void)
{
	Node *ret;

	ret = malloc(sizeof *ret);
	if (!ret) return NULL;

	make_vector(ret->edge);
	if (!ret->edge) goto nomem;
	make_vector(ret->chld);
	if (!ret->chld) goto nomem;

	return ret;

nomem:
	free_vector(ret->edge);
	free_vector(ret->chld);
	return NULL;
}

int
attach(Node *cur, Node const *new)
{
	int cmp;
	size_t min;
	size_t off;
	Node *chld;

	for (off = 0; off < len(cur->chld); ++off) {
		chld = arr(cur->chld)[off];
		min = MIN(len(chld->edge), len(new->edge));
		cmp = memcmp(arr(new->edge), arr(chld->edge), min);
		if (cmp < 0) break;
	}

	return vec_insert(cur->chld, &new, off);
}

void
delete(Node *cur, Node const *targ)
{
	size_t off;

	if (!targ) {
		vec_truncate(cur->edge, 0);
		return;
	}

	for (off = 0; off < len(cur->chld); ++off) {
		if (arr(cur->chld)[off] == targ) {
			vec_delete(cur->chld, off);
			return;
		}
	}
}

int
insert(Node *nod, Elem const *el)
{
	vec_truncate(nod->edge, 0);
	return vec_join(nod->edge, el);
}

void
free_node(Node *nod)
{
	size_t i;

	for (i = 0; i < len(nod->chld); ++i)
		free_node(arr(nod->chld)[i]);

	free_vector(nod->chld);
	free_vector(nod->edge);
	memset(nod, 0, sizeof *nod);
	free(nod);
}

Node **
locate(Node *nod, Elem const *el)
{
	size_t off;
	Elem *pre;
	Node **res;
	Node *cur;
	Node *prev;

	res = calloc(2, sizeof *res);
	if (!res) return NULL;
	pre = vec_clone(el);
	if (!pre) {
		free(res);
		return NULL;
	}

	cur = nod;
	prev = NULL;
	for (;;) {
		off = match(cur, pre);
		if (off != len(cur->edge)) break;

		vec_shift(pre, off);
		if (!len(pre) && isleaf(cur)) {
			res[0] = prev;
			res[1] = cur;
			break;
		}

		prev = cur;
		cur = search(cur, pre);
		if (!cur) break;
	}

	return res;
}

void *
marshal(Node const *nod, Elem const *el)
{
	size_t i;
	Elem *pre;
	List(uint8_t *) *ret, *tmp;

	pre = vec_clone(el);
	if (!pre) return NULL;

	make_list(ret);
	if (!ret) {
		free(pre);
		return NULL;
	}

	vec_join(pre, nod->edge);
	for (i = 0; i < len(nod->chld); ++i) {
		tmp = marshal(arr(nod->chld)[i], pre);
		if (!tmp) {
			free_vector(pre);
			free_list(ret);
			return NULL;
		}
		list_append(ret, tmp);
	}

	if (isleaf(nod)) {
		tmp = list_cons(&arr(pre), ret);
		if (!tmp) {
			mapl(ret, free(each));
			free_list(ret);
		}
		ret = tmp;
	}
	free(pre);

	return ret;
}

size_t
match(Node const *nod, Elem const *el)
{
	size_t off;

	for (off = 0; off < MIN(len(nod->edge), len(el)); ++off)
		if (arr(nod->edge)[off] != arr(el)[off])
			break;

	return off;
}

Node *
search(Node const *nod, Elem const *el)
{
	size_t i;

	if (!len(el)) {
		if (!len(arr(nod->chld)[0]->edge))
			return arr(nod->chld)[0];
		else return NULL;
	}

	for (i = 0; i < len(nod->chld); ++i)
		if (*arr(arr(nod->chld)[i]->edge) == *arr(el))
			return arr(nod->chld)[i];

	return NULL;
}

int
split(Node *nod, size_t off)
{
	int err;
	size_t len;
	uint8_t *tmp;
	Node *new;

	len = len(nod->edge) - off;
	if (!len) return 0;
	tmp = malloc(len);
	if (!tmp && len) return ENOMEM;
	new = alloc_node();
	if (!new) {
		free(tmp);
		return ENOMEM;
	}

	memcpy(tmp, arr(nod->edge) + off, len);
	vec_truncate(nod->edge, off);
	err = vec_concat(new->edge, tmp, len);
	if (err) return err;

	return attach(nod, new);
}

Reply *
traverse(Node *nod, Elem const *el)
{
	size_t ext = 0;
	size_t min;
	size_t off;
	Elem *pre;
	Node *cur;
	Node *tmp;
	Reply *ret;

	ret = calloc(1, sizeof *ret);
	if (!ret) return NULL;
	pre = vec_clone(el);
	if (!pre) {
		free(ret);
		return NULL;
	}

	cur = nod;
	for (;;) {
		min = MIN(len(cur->edge), len(pre));
		off = match(cur, pre);
		if (off != min) break;

		ext += off;
		vec_shift(pre, off);
		if (!len(pre)) break;

		tmp = search(cur, pre);
		if (!tmp) break;
		cur = tmp;
	}

	ret->ext = ext;
	ret->off = off;
	ret->nod = cur;
	return ret;
}

Elem *
make_elem(void const *data, size_t len)
{
	Elem *ret;

	make_vector(ret);
	if (!ret) return NULL;

	if (vec_concat(ret, data, len)) {
		free_vector(ret);
		return NULL;
	}

	return ret;
}

Set *
set_alloc(void)
{
	Set *ret;

	ret = calloc(1, sizeof *ret);
	if (!ret) return NULL;

	ret->root = alloc_node();
	if (!ret->root) {
		free(ret);
		return NULL;
	}

	return ret;
}

void
set_free(Set *A)
{
	free_node(A->root);
	memset(A, 0, sizeof *A);
	free(A);
}

int	set_adds   (Set *A, char *s) { return set_add   (A, s, strlen(s) + 1); }
int	set_rms	   (Set *A, char *s) { return set_rm    (A, s, strlen(s) + 1); }
bool	set_membs  (Set *A, char *s) { return set_memb  (A, s, strlen(s) + 1); }
void *	set_querys (Set *A, char *s) { return set_query (A, s, strlen(s)); }

int
set_add(Set *A, void *data, size_t len)
{
	int err = ENOMEM;
	size_t off;
	Elem *el;
	Node *new = NULL;
	Node *par;
	Reply *rep = NULL;

	el = make_elem(data, len);
	if (!el) goto fail;
	
	rep = traverse(A->root, el);
	if (!rep) goto fail;
	par = rep->nod;
	off = rep->off;
	vec_shift(el, rep->ext);

	err = split(par, off);
	if (err) goto fail;

	new = alloc_node();
	if (!new) goto fail;

	vec_shift(el, off);
	err = insert(new, el);
	if (err) goto fail;

	err = attach(par, new);
	if (err) goto fail;

	err = align(par);
	if (err) goto fail;

	free_vector(el);
	return 0;
fail:
	free_vector(el);
	free_node(new);
	free(rep);
	return err;
}

int
set_rm(Set *A, void *data, size_t len)
{
	Elem *el;
	Node **res;

	el = make_elem(data, len);
	if (!el) return ENOMEM;

	res = locate(A->root, el);
	if (!res[1]) {
		free_vector(el);
		return ENOENT;
	}

	delete(res[1], res[0]);

	free(res);
	return 0;
}

bool
set_memb(Set *A, void *data, size_t len)
{
	bool ret;
	Elem *el;
	Node **res;

	el = make_elem(data, len);
	if (!el) return ENOMEM;

	res = locate(A->root, el);
	ret = !!res[1];

	free(res);
	free_vector(el);

	return ret;
}

void *
set_query(Set *A, void *data, size_t len)
{
	Elem *el;
	Reply *rep = NULL;
	List(uint8_t *) *ret = NULL;

	el = make_elem(data, len);
	if (!el) goto finally;

	rep = traverse(A->root, el);
	if (!rep) goto finally;
	if (rep->ext != len(el)) {
		make_list(ret);
		if (!ret) goto finally;
		return ret;
	}
	
	vec_truncate(el, rep->ext - rep->off);
	ret = marshal(rep->nod, el);

finally:
	free(rep);
	free_vector(el);
	return ret;
}
