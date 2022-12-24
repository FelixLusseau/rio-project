#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
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

#define BUFSIZE 1024
#define MAXCLIENTS 10
#define LENNOM 11

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
 * @brief Retire le nom entre ':' du message
 *
 * @param msg
 * @param j
 */
void remove_nom(char *msg, int j) {
    for (int i = j + 1; i < BUFSIZE && msg[i - 1] != '\0'; i++) {
        msg[i - j - 2] = msg[i];
    }
}

/**
 * @brief regarde si c'est un broadcast ou un destinataire;
 * retourne 0 si broadcast et 1 sinon;
 *
 * @param msg
 * @param nom
 * @return int
 */
int chk_nom(char *msg, char *nom) {
    int j;
    for (int i = 1; i < 10 && msg[i] != ':'; i++) {
        nom[i - 1] = msg[i];
        j = i;
    }
    nom[j] = '\0';
    if (msg[0] != ':') {
        return 0;
    } else {
        remove_nom(msg, j);
        return 1;
    }
}

int main(int argc, char **argv) {
    // vérification du nombre d'arguments sur la ligne de commande
    if (argc != 2) {
        raler(0, "Usage: %s port_local\n", argv[0]);
    }

    int sockfd;        // descripteur de socket
    fd_set fd_monitor; // ensemble des descripteurs à surveiller
    int retval;        // valeur de retour de la fonction select
    char nom_correct;  // vrai si le nom est correct

    char buf[BUFSIZE + 50]; // espace nécessaire pour stocker le message à envoyer + mise en forme du nom de l'expéditeur
    char bufr[BUFSIZE];     // espace nécessaire pour stocker le message recu

    // taille d'une structure sockaddr_in utile pour la fonction recv
    socklen_t fromlen = sizeof(struct sockaddr_in);

    struct sockaddr_in my_addr;             // structure d'adresse qui contiendra les params réseaux du récepteur
    struct sockaddr_in clients[MAXCLIENTS]; // structure d'adresse qui contiendra les params réseaux des clients
    char noms[MAXCLIENTS][LENNOM];          // tableau contenant les noms des clients
    int s[MAXCLIENTS], max_fd;              // tableau de descripteurs de sockets et descripteur de socket maximum
    int i = 0;
    ssize_t r; // nombre de caractères lus par la fonction recv

    // création de la socket de connexion
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // famille d'adresses
    my_addr.sin_family = AF_INET;

    // récuperation du port du récepteur
    my_addr.sin_port = htons(atoi(argv[1]));

    // adresse IPv4 du récepteur
    inet_aton("127.0.0.1", &(my_addr.sin_addr));

    // association de la socket et des params réseaux du récepteur
    CHK(bind(sockfd, (void *)&my_addr, fromlen));

    while (1) {
        // mise à jour de l'ensemble des descripteurs à surveiller
        FD_ZERO(&fd_monitor); // bug parfois si hors de la boucle
        max_fd = sockfd;
        FD_SET(sockfd, &fd_monitor);
        for (int k = 0; k < i; k++) {
            if (s[k] > 0) { // si le descripteur de socket est valide et non encore fermé (!= -1)
                FD_SET(s[k], &fd_monitor);
                if (s[k] > max_fd)
                    max_fd = s[k];
            }
        }
        FD_SET(STDIN_FILENO, &fd_monitor);

        CHK(retval = select(max_fd + 1, &fd_monitor, NULL, NULL, NULL));

        // initialisation d'une nouvelle connexion
        if (FD_ISSET(sockfd, &fd_monitor)) {
            CHK(listen(sockfd, MAXCLIENTS));
            CHK(s[i] = accept(sockfd, (void *)&clients[i], &fromlen));

            CHK(send(s[i], "\e[0;32mBienvenue, quel est ton nom ? (max 10 caractères)\e[0m\n", 63, 0));
            CHK(r = recv(s[i], &noms[i], LENNOM, 0));
            noms[i][r - 2] = '\0';

            FD_CLR(s[i], &fd_monitor);
            r = snprintf(buf, BUFSIZE, "\e[0;32m%s a rejoint le chat !\e[0m\n", noms[i]);
            printf("%s", buf);
            for (int l = 0; l < i + 1; l++)
                if (l != i && s[l] != -1)
                    CHK(send(s[l], buf, strlen(buf) + 1, 0));
            i++;

            // affichage des noms des clients connectés
            snprintf(buf, BUFSIZE, "\e[0;36mUtilisateurs actuellement connectés : \n");
            for (int k = 0; k < i; k++) {
                if (s[k] != -1) {
                    strcat(buf, " - ");
                    strcat(buf, noms[k]);
                    strcat(buf, "\n");
                }
            }
            strcat(buf, "\e[0m\n");
            printf("%s", buf);
            for (int l = 0; l < i; l++)
                if (s[l] != -1)
                    CHK(send(s[l], buf, strlen(buf) + 1, 0));
        }

        // lecture des messages sur toutes les sockets connectées
        for (int j = 0; j < i; j++) {
            if (FD_ISSET(s[j], &fd_monitor)) {
                CHK(r = recv(s[j], bufr, BUFSIZE, 0));
                bufr[r] = '\0';

                char nombuf[LENNOM];
                if (chk_nom(bufr, nombuf)) { // si un destinataire est désigné
                    snprintf(buf, BUFSIZE + 50, "[%s (en privé)] : %s", noms[j], bufr);
                    nom_correct = 0;
                    for (int t = 0; t < i; t++) {
                        if (!strcmp(nombuf, noms[t]) && s[t] != -1) {
                            nom_correct = 1;
                            CHK(send(s[t], buf, strlen(buf) + 1, 0));
                        }
                    }
                    if (!nom_correct)
                        CHK(send(s[j], "\e[0;33mCe destinataire n'existe pas !\e[0m\n", 43, 0));
                    nom_correct = 0;

                } else if (strcmp(bufr, "/quit") == 0 || strcmp(bufr, "/quit\n") == 0) { // si un client vient de quitter le chat
                    close(s[j]);
                    s[j] = -1;
                    snprintf(buf, BUFSIZE + 24, "\e[0;31m%s a quitté le chat !\e[0m\n", noms[j]);
                    printf("%s", buf);

                    for (int l = 0; l < i; l++)
                        if (l != j && s[l] != -1)
                            CHK(send(s[l], buf, strlen(buf) + 1, 0));

                } else { // sinon affichage de la chaîne de caractères reçue et retransmission à tous les autres clients en broadcast
                    snprintf(buf, BUFSIZE + 24, "[%s] : %s", noms[j], bufr);
                    printf("%s", buf);
                    for (int l = 0; l < i; l++) {
                        if (l != j && s[l] != -1)
                            CHK(send(s[l], buf, strlen(buf) + 1, 0));
                    }
                }
            }
        }

        // saisie d'un message sur l'entrée standard du serveur et envoi en broadcast
        if (FD_ISSET(STDIN_FILENO, &fd_monitor)) {
            NCHK(fgets(bufr, BUFSIZE, stdin));
            if (strcmp(bufr, "/quit\n") == 0 || strcmp(bufr, "/quit") == 0) {
                CHK(close(sockfd));
                for (int k = 0; k < i; k++) {
                    if (s[k] > 0) {
                        CHK(close(s[k]));
                    }
                }
                return 0;
            }

            snprintf(buf, BUFSIZE + 24, "[%s] : %s", "Serveur", bufr);
            for (int j = 0; j < i; j++)
                if (s[j] != -1)
                    CHK(send(s[j], buf, strlen(buf) + 1, 0));
        }
    }

    return 0;
}
