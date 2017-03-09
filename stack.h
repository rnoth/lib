#pragma once

typedef struct Stack Stack;

struct Stack {
	size_t h;
	size_t m;
	void **v;
};

void freestack(Stack *);
int stack_push(Stack *, void *);
void *stack_pop(Stack *);
int makestack(Stack **);
void maps(Stack *, void (*)(void *));
