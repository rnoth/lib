simple library implementing serveral generic datatypes for C programming

##vec

the vec.h functions are built on top of ordinary C arrays, so a vector of type Type would be declared as normal: `Type *vec`. These 'vectors' can interoperate with functions expecting normal arrays, but the reverse is not true.

the 'make\_vector()' macro will magically initialize the vector for you.
Use it like: `make_vector(vec);`

- instance is o (or `NULL`) if an allocation fails

to allocate a vector manually, just do: `vec_alloc(vec, sizeof *vec)`.
This function return `ENOMEM` if the allocation fails.

free a vector with `vec_free(vec);`

###supported operations:

**important**: all of these functions take pointers to vectors allocated with `make_vector()` (or `vec_alloc()`.
if you just pass the vector itself, your program might blow up.

- `vec_append(void *vec_ptr, void *item);`

- `vec_insert(void *vec_ptr, void *item, size_t position);`

	- item should be a pointer of the same type as the vector,
	that is, `int *vector` -> `int *item`

	- these functions return 0 on succces, `ENOMEM` if they cannot allocate memory, `EOVERFLOW` if some value would overflow (this should never happen).

	- `vec_insert()` can also return `EINVAL` if the position is out of bounds

- `vec_delete(void *vec_ptr, size_t position)`

- `vec_truncate(void *vec_ptr, size_t offset)`

- `vec_shift(void *vec_ptr, size_t offset)`

- `vec_slice(void *vec_ptr, size_t begin, size_t extent)`
	
- `vec_concat(void *vec_ptr, void *array, size_t length)`

	- array should be an array of the same type as the vector

	- length is the length of the array in type-units, not bytes
	  (\`concat'ing an array of five ints, length would be \`5', not \`20')

	- return 0 on succes, `ENOMEM` when out of memory

- `vec_join(void *dest_ptr, void *src_ptr)`

	- appends the elements of `*src_ptr` to `*dest`

	- return 0 on success, `ENOMEM` when out of memory

- `vec_clone(void *vec_ptr)`

	- returns a copy of vector, or 0 (`NULL`) if allocation fails

###fun

- `mapv(variable, vector)` is a macro which loops over the elements of `vector`,
assigning the address of each one to `varible` in sequence. e.g.

```
int
main()
{
	int *foo;

	make_vector(foo);
	if (!foo) abort();

	/* ... */

	mapv(int *each, foo) {
		printf("%d^2 == %d\n", *each, *each * *each);
	}

	/* ... */

	return 0;
}
```

	- as seen above, you can declare variables inside mapv(), like a for loop initializer

	- furthermore, `continue` and `break` behave as expected

##list

declare a singly-linked list of type Type with `List(Type) *instance`.
note that lists do not store pointers like vectors, and therefore
`List(void)` is invalid and a list of cstrings would be `List(char *)`

alloc & initialize a vector with `make_list(instance)`

- instance is NULL if the allocation fails

free a list with `free_list(instance)`

###supported operations:

- `car(list)` -- macro which expands to `list->val`

- `cdr(list)` -- macro which expands to `list-\>next`

note that next pointer is stored as void, meaning constructions such as
`cdr(cdr(list))` will not work

- `list_cons(void *item, void *list)`

	- item should be a pointer to the type which the list was declared

- `list_append(void *destination, void *source)`

###fun:

- `mapl(list, expression)` is a macro which applies expression to each
item in the list. the currect item is can be accessed in the `each` variable
e.g. `mapl(list, free(each))` free every element in the list

	- note that this macro assumes the list is of pointers, and will not
	work for scalar or struct types

##set
declare a radix tree with `Set *instance`

alloc & initialize:

- `instance = set_alloc()`

free:

- `set_free(instance)`

###supported operations:

- `set_add(Set *set, void *element, size_t length)`

- `set_rm(Set *set, void *element, size_t length)`

- `set_memb(Set *set, void *element, size_t length)`

	- return true iff element is in set

- `set_prefix(Set *set, void *prefix, size_t length)`

	- return true iff there are elements with the prefix prefix

- `set_query(Set *set, void *prefix, size_t length)`

	- returns a vector of elements with the prefix prefix

every operation has a version design for use with cstrings, they are
`set_adds()`, `set_rms()`, `set_membs()`, and `set_querys()`, respectively
