all: prog1 prog2 prog3 prog4 progExtra1 progExtra2 progExtra3 progExtra4
prog1: prog1.c
	gcc -Wall -fsanitize=address,undefined -o prog1 prog1.c
prog2: prog2.c
	gcc -Wall -fsanitize=address,undefined -o prog2 prog2.c
prog3: prog3.c
	gcc -Wall -fsanitize=address,undefined -o prog3 prog3.c
prog4: prog4.c
	gcc -Wall -fsanitize=address,undefined -o prog4 prog4.c
progExtra1: progExtra1.c
	gcc -Wall -fsanitize=address,undefined -o progExtra1 progExtra1.c
progExtra2: progExtra2.c
	gcc -Wall -fsanitize=address,undefined -o progExtra2 progExtra2.c
progExtra3: progExtra3.c
	gcc -Wall -fsanitize=address,undefined -o progExtra3 progExtra3.c
progExtra4: progExtra4.c
	gcc -Wall -fsanitize=address,undefined -o progExtra4 progExtra4.c
.PHONY: clean all
clean:
	rm prog1 prog2 prog3 prog4 progExtra1 progExtra2 progExtra3 progExtra4
