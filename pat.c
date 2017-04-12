#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "pat.h"
#include "vec.h"

enum pattype {
	ALTER,	// |
	LITERAL,
	OPT,	// ?
	STAR,	// *
	PLUS,	// +
	LPAREN,	// (
	RPAREN,	// )
};

enum patop {
	HALT = 0x0,
	CHAR = 0x1,
	FORK = 0x2,
	JUMP = 0x3,
	MARK = 0x4,
	SAVE = 0x5,
};

typedef enum pattype pattype_e;
typedef enum patop patop_e;
typedef struct pattok pattok_t;
typedef struct patins patins_t;

static pattok_t patlex(char const *);
static int pataddalt(pattern_t *, pattok_t *, patins_t **);
static int pataddlpar(pattern_t *, pattok_t *, patins_t **);
static int pataddrpar(pattern_t *, pattok_t *, patins_t **);
static int pataddrep(pattern_t *, pattok_t *, patins_t **);

struct pattok {
	size_t len;
	pattype_e type;
	int (*eval)(pattern_t *, pattok_t *, patins_t **);
};

struct patins {
	patop_e op;
	uint64_t arg;
};

int
pataddalt(pattern_t *pat, pattok_t *tok, patins_t **tmp)
{
	int err = 0;
	size_t offset = len(*tmp) + 2;
	patins_t ins = {0};

	if (offset < len(*tmp)) return EOVERFLOW;

	ins.op = FORK;
	ins.arg = offset;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	err = vec_join(&pat->prog, *tmp);
	if (err) return err;

	ins.op = JUMP;
	ins.arg = -1;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	vec_truncate(tmp, 0);

	return 0;
}

int
pataddlpar(pattern_t *pat, pattok_t *tok, patins_t **tmp)
{
	int err = 0;
	patins_t ins = {0};

	err = vec_join(&pat->prog, *tmp);
	if (err) return err;

	ins.op = JUMP;
	ins.arg = -2;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	return 0;
}

int
pataddrep(pattern_t *pat, pattok_t *tok, patins_t **tmp)
{
	int err = 0;
	patins_t ins = {0};
	size_t again = len(pat->prog);
	size_t offset = tok->type != OPT ? 3 : 2;

	if (again < len(pat->prog)) return EOVERFLOW;

	if (!len(*tmp)) return EILSEQ;

	err = vec_concat(&pat->prog, *tmp, len(*tmp) - 1);
	if (err) return err;

	if (tok->type != PLUS) {
		ins.op = FORK;
		ins.arg = len(pat->prog) + offset;

		if (ins.arg < len(pat->prog)) return EOVERFLOW;

		err = vec_append(&pat->prog, &ins);
		if (err) return err;
		++again;
	}

	ins = *tmp[len(*tmp) - 1];
	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	if (tok->type != OPT) {
		ins.op = FORK;
		ins.arg = again;
		err = vec_append(&pat->prog, &ins);
		if (err) return err;
	}

	vec_truncate(tmp, 0);
	return 0;
}

int
pataddrpar(pattern_t *pat, pattok_t *tok, patins_t **tmp)
{
	int err = 0;
	size_t i = 0;
	uint64_t u = 0;
	patins_t ins = {0};
	patins_t *insp = 0x0;

	err = vec_join(&pat->prog, *tmp);
	if (err) return err;

	i = len(pat->prog);
	while (i --> 0) {
		insp = pat->prog + i;
		if (insp->op == JUMP && insp->arg > -3UL) {
			u = insp->arg;
			insp->arg = len(pat->prog) + 1;
			if (u == -2UL) break;
		}
	}

	ins.op = JUMP;
	ins.arg = len(pat->prog) + 2;
	if (ins.arg < len(pat->prog)) return EOVERFLOW;

	err = vec_append(&pat->prog, &ins);
	if (err) return EOVERFLOW;

	ins.op = JUMP;
	ins.arg = i + 1;
	if (ins.arg < i) return EOVERFLOW;

	err = vec_append(&pat->prog, &ins);
	if (err) return err;

	vec_truncate(tmp, 0);

	return 0;
}

int
patcomp(pattern_t *dest, char const *src)
{
	int err = 0;
	patins_t *tmp = 0x0;
	wchar_t wc = 0;
	pattok_t tok = {0};
	patins_t ins = {0};

	vec_ctor(tmp);
	if (!tmp) return ENOMEM;

	vec_ctor(dest->prog);
	if (!dest->prog) return ENOMEM;

	while (*src) {
		tok = patlex(src);
		if (tok.type == LITERAL) {
			ins.op = CHAR;
			// TODO: this is incorrect for escaped chars
			err = mbtowc(&wc, src, tok.len);
			if (err == -1) return -1;
			ins.arg = (uint64_t)wc;
			vec_append(&tmp, &ins);
		} else {
			err = tok.eval(dest, &tok, &tmp);
			if (err) goto finally;
		}
		src += tok.len;
	}

finally:
	vec_free(tmp);
	if (err) vec_free(dest->prog);
	return err;
}

int
patexec(patmatch_t **matv, char const *str, pattern_t const *pat)
{
	return -1;
}

void
patfree(pattern_t *pat)
{
}

pattok_t
patlex(char const *src)
{
	pattok_t ret;

	switch (src[0]) {
	case '?':
		ret.type = OPT;
		ret.len = 1;
		ret.eval = pataddrep;
		break;
	case '*':
		ret.type = STAR;
		ret.len = 1;
		ret.eval = pataddrep;
		break;
	case '+':
		ret.type = PLUS;
		ret.len = 1;
		ret.eval = pataddrep;
		break;
	case '(':
		ret.type = LPAREN;
		ret.len = 1;
		ret.eval = pataddlpar;
		break;
	case ')':
		ret.type = RPAREN;
		ret.len = 1;
		ret.eval = pataddrpar;
		break;
	case '|':
		ret.type = ALTER;
		ret.len = 1;
		ret.eval = pataddalt;
		break;
	case '\\':
		goto esc;
	default:
		ret.type = LITERAL;
		ret.len = mblen(src, strlen(src));
	}

	return ret;

esc:
	switch (src[1]) {
	default:
		ret.type = LITERAL;
		ret.len = 1 + mblen(src, strlen(src + 1));
	}

	return ret;
}

int
patmatch(patmatch_t **matv, char const *str, char const *pat, long opts)
{
	return -1;
}
