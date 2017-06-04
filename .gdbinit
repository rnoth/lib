set logging redirect on
set logging file /dev/null
set logging on
skip node
skip leaf
skip isnode
skip isleaf
skip is_leaf
skip is_node
skip tag_node
skip tag_leaf
skip to_node
skip to_leaf


skip file vec.h
skip file vec.c

skip ctx_init
skip ctx_fini

handle SIGALRM ignore
set logging off

define print-prog
	set $len = vec_len(pat->prog)
	set $i = 0
	while $i < $len
		x/a &pat->prog[$i].op
		printf "\e[[A\r.op =\n"
		print pat->prog[$i].arg.f
		printf "\e[[A\r.arg =\n"
		set $i += 1
	end
end
