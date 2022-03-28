ww: ww.o
	gcc -Wall -fsanitize=address $^ -o $@

%.o: %.c
	gcc -c -Wall -fsanitize=address $<