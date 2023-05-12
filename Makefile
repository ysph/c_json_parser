all:
	gcc -Wall -Wextra -g -Wpedantic -std=c99 -o main main.c

map:
	gcc -Wall -Wextra  -Wpedantic -std=c99 -o main2 main2.c

#-Werror