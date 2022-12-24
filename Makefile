all:
	gcc -o serveur serveur.c -Wall -Wextra -Werror -g
	gcc -o client client.c -Wall -Wextra -Werror -g

clean:
	rm -f serveur client