archiver: archiver.c
	gcc -o -fsanitize=address archiver archiver.c

