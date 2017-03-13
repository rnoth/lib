simple library implementing serveral generic datatypes for C programming

##vec
declare a dynamic array of type Type with `Vector(Type) *instance;`.

- vectors should be pointers

alloc & initialize a vector with `make_vector(instance);`

- instance is `NULL` if an allocation fails

free a vector with `vec_free(instance);`

###supported operations:

- `vec_append(void *vector, void *item);`

- `vec_prepend(void *vector, void *item);`

- `vec_insert(void *vector, void *item, size_t position);`

	- item should be a pointer of the same type as the vector,
	that is, `Vector(int) *vector` -> `int *item`

	- these functions return 0 on succces, `ENOMEM` if they cannot allocate memory

- `vec_delete(void *vector, size_t position)`

- `vec_truncate(void *vector, size_t offset)`

- `vec_shift(void *vector, size_t offset)`

- `vec_slice(void *vector, size_t begin, size_t extent)`
	
- `vec_concat(void *vector, void *array, size_t length)`

	- array should be an array of the same type as the vector

	- length is the length of the array in type-units, not bytes
	  (`concat'ing an array of five ints, length would be `5', not `20')

	- return 0 on succes, `ENOMEM` when out of memory

- `vec_join(void *destination, void *source)`

	- appends the elements of source to destination

	- return 0 on success, `ENOMEM` when out of memory

- `vec_clone(void *vector)`

	- returns a copy of vector, or `NULL` if allocation fails

##list
declare a singly-linked list of type Type with `List(Type) *instance`.
note that lists do not store pointers like vectors, and therefore
`List(void)` is invalid and a list of cstrings would be `List(char *)`

alloc & initialize a vector with `make_list(instance)`

- instance is NULL if the allocation fails

free a list with `free_list(instance)`

###supported operations:

- `car(list)` -- macro which expands to list-\>val

- `cdr(list)` -- macro which expands to list-\>next

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

- `set_query(Set *set, void *prefix, size_t length)`

	- returns a list of elements with the prefix prefix

every operation has a version design for use with cstrings, they are
`set_adds()`, `set_rms()`, `set_membs()`, and `set_querys()`, respectively
