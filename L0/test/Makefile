all: prog1 prog2 prog3 prog4 prog5 prog6 prog7 prog8
prog1: prog1.c
	gcc -Wall -fsanitize=address,undefined -o prog1 prog1.c
prog2: prog2.c
	gcc -Wall -fsanitize=address,undefined -o prog2 prog2.c
prog3: prog3.c
	gcc -Wall -fsanitize=address,undefined -o prog3 prog3.c
prog4: prog4.c
	gcc -Wall -fsanitize=address,undefined -o prog4 prog4.c
prog5: prog5.c
	gcc -Wall -fsanitize=address,undefined -o prog5 prog5.c
prog6: prog6.c
	gcc -Wall -fsanitize=address,undefined -o prog6 prog6.c
prog7: prog7.c
	gcc -Wall -fsanitize=address,undefined -o prog7 prog7.c
prog8: prog8.c
	gcc -Wall -fsanitize=address,undefined -o prog8 prog8.c
.PHONY: clean all
clean:
	rm prog1 prog2 prog3 prog4 prog5 prog6 prog7 prog8
