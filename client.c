#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define CHK(op)            \
    do {                   \
        if ((op) == -1)    \
            raler(1, #op); \
    } while (0)
#define NCHK(op)           \
    do {                   \
        if ((op) == NULL)  \
            raler(0, #op); \
    } while (0)

#define MAXNOM 10
#define BUFSIZE 1024

/**
 * @brief cette fonction permet d'écrire un message dans la sortie erreur.
 * si syserr vaut 1, la fonction est appelée à cause d'une primitive système qui a
 * échoué et va donc faire perror() et écrire le message const char *msg.
 * si syserr vaut 0, la fonction est appelée pour une autre erreur et affiche seulement le
 * messagé écrit dans le const char *msg
 *
 * @param syserr
 * @param msg
 * @param ...
 * @return noreturn
 */
noreturn void raler(int syserr, const char *msg, ...) {
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    if (syserr == 1)
        perror("");

    exit(EXIT_FAILURE);
}

/**
 * @brief vérifie si l'écriture du message est bonne
 * retourne 1 si oui, 0 sinon.
 *
 * @param msg
 * @return int
 */
int CK_taille_nom(char *msg) {
    int taille = strlen(msg);
    if (msg[0] != ':')
        return 1;
    for (int i = 1; i < taille && i < 11; i++) {
        if (msg[i] == ':') {
            if (i + 2 == taille) {
                printf("Il faut un message\n");
                return 0;
            } else {
                return 1;
            }
        }
    }
    printf("usage: :nom_destinataire:votre_message avec le nom du destinataire inferieur à 10 caractères\n");
    return 0;
}

int main(int argc, char **argv) {
    // vérification du nombre d'arguments sur la ligne de commande
    if (argc != 3) {
        raler(0, "usage: %s @dest num_port\n", argv[0]);
    }

    int sockfd;              // descripteur de socket
    struct sockaddr_in dest; // structure d'adresse qui contiendra les paramètres réseaux du destinataire

    fd_set fd, fd_set;

    // création de la socket
    CHK(sockfd = socket(AF_INET, SOCK_STREAM, 0));

    // famille d'adresses
    dest.sin_family = AF_INET;

    // adresse IPv4 du destinataire
    if (inet_aton(argv[1], &(dest.sin_addr)) == 0) {
        raler(0, "inet_aton: adresse non valide");
    }

    // port du destinataire
    dest.sin_port = htons(atoi(argv[2]));

    CHK(connect(sockfd, (const struct sockaddr *)&dest, sizeof(struct sockaddr)));

    // le client donne son nom dans l'entrée standard
    char buffer[BUFSIZE];
    ssize_t r;
    CHK(recv(sockfd, buffer, BUFSIZE, 0));
    printf("%s", buffer);
    NCHK(fgets(buffer, BUFSIZE, stdin));
    while (strlen(buffer) > 10) {
        printf("\e[0;33mVotre nom doit faire moins de 10 caractères !\e[0m\n");
        NCHK(fgets(buffer, BUFSIZE, stdin));
    }

    // envoi du nom au serveur
    CHK(send(sockfd, buffer, strlen(buffer) + 1, 0));

    FD_ZERO(&fd_set);
    FD_SET(sockfd, &fd_set);
    FD_SET(0, &fd_set);
    printf("\e[0;35mPour envoyer un message, écrire 'le_message' ou bien ':nom_destinataire:le_message' pour une personne spécifique\e[0m\n");

    // envoi des messages
    while (1) {
        fd = fd_set;
        CHK(select(sockfd + 1, &fd, NULL, NULL, NULL));
        // on reçoit un message du serveur
        if (FD_ISSET(sockfd, &fd)) {
            CHK(r = recv(sockfd, buffer, BUFSIZE, 0));
            buffer[r] = '\0';
            printf("%s", buffer); // on affiche le message du serveur
        }
        // on écrit sur l'entrée standard
        if (FD_ISSET(0, &fd)) {
            NCHK(fgets(buffer, BUFSIZE, stdin));
            if (strcmp(buffer, "/quit\n") == 0 || strcmp(buffer, "/quit") == 0) {
                // Si le client veut partir, on le signale au serveur
                CHK(send(sockfd, buffer, strlen(buffer) + 1, 0));
                CHK(close(sockfd));
                return 0;
            }
            // on envoie le message seulement si l'écriture du message est bonne
            if (CK_taille_nom(buffer)) {
                CHK(send(sockfd, buffer, strlen(buffer) + 1, 0));
            }
        }
    }
    return 0;
}
