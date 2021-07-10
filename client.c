#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>


#define BUFLEN 500

typedef enum
{
    NAC     = -1,
    SAO     = -2,
    WPIN    = -3,
    CNF     = -4,
    CBK     = -5,
    OPF     = -6,
    UBF     = -7,
    IFU     = -8,
    OPC     = -9,
    FERR    = -10
} ibank_error_t;


bool exitflag;
bool logged;
bool transferop;
bool unlockop;
int lastcard;


void error(ibank_error_t code, FILE *log, char *msg)
{
	switch (code)
    {
    case NAC:
        fprintf(log, "%d : The client is not authenticated\n", NAC);
        printf("%d : The client is not authenticated\n", NAC);
        break;
    case SAO:
        fprintf(log, "%d : Session already opened\n", SAO);
        printf("%d : Session already opened\n", SAO);
        break;
    case WPIN:
        fprintf(log, "%d : Wrong PIN\n", WPIN);
        printf("%d : Wrong PIN\n", WPIN);
        break;
    case CNF:
        fprintf(log, "%d : Card number doesn't exist\n", CNF);
        printf("%d : Card number doesn't exist\n", CNF);
        break;
    case CBK:
        fprintf(log, "%d : Card number blocked\n", CBK);
        printf("%d : Card number blocked\n", CBK);
        break;
    case OPF:
        fprintf(log, "%d : Operation failed\n", OPF);
        printf("%d : Operation failed\n", OPF);
        break;
    case UBF:
        fprintf(log, "%d : Unblocking failed\n", UBF);
        printf("%d : Unblocking failed\n", UBF);
        break;
    case IFU:
        fprintf(log, "%d : Insuficient funds\n", IFU);
        printf("%d : Insuficient funds\n", IFU);
        break;
    case OPC:
        fprintf(log, "%d : Operation canceled\n", OPC);
        printf("%d : Operation canceled\n", OPC);
        break;
    case FERR:
        fprintf(log, "%d : Function error: %s\n", FERR, msg);
        printf("%d : Function error: %s\n", FERR, msg);
        exit(1);
        break;
    default:
        break;
	}
}

void clean_exit(int tcp_socket, int udp_socket, FILE* log)
{
    if (close(tcp_socket) < 0)
        error(FERR, log, "close()");
    if (close(udp_socket) < 0)
        error(FERR, log, "close()");
    if (fclose(log) < 0)
        error(FERR, log, "fclose()");
}

int mastercommands(char *cmd)
{
    char *token;

    char deep_copy[BUFLEN];
    strcpy(deep_copy, cmd);

    token = strtok(cmd, " \n");

    if (strcmp(token, "login") != 0 
        && strcmp(token, "logout") != 0 
        && strcmp(token, "listsold") != 0 
        && strcmp(token, "transfer") != 0
        && strcmp(token, "unlock") != 0
        && strcmp(token, "quit") != 0
        && !transferop && !unlockop)
    {
        printf("CLIENT> Invalid command.\n");
        return 0;
    }

    if (strcmp(token, "quit") == 0)
    {
        exitflag = true;
        return 1;
    }

    if (unlockop)
    {
        char cp[BUFLEN - sizeof(int) - sizeof(char)];
        strcpy(cp, cmd);
        memset(cmd, 0 , BUFLEN);
        sprintf(cmd, "%d %s", lastcard, cp);
        unlockop = false;
        return 2;
    }

    if (strcmp(token, "unlock") == 0)
    {
        if (logged)
            return SAO;
        if (lastcard == -1)
            return OPF;

        unlockop = true;
        memset(cmd, 0 , BUFLEN);
        sprintf(cmd, "unlock %d\n", lastcard);
        return 2;
    }

    if (strcmp(token, "login") == 0)
    {
        char* card_nr = strtok (NULL, " \n");
        lastcard = atoi(card_nr);
    }

    if (strcmp(token, "login") != 0
        && logged == false
        && !unlockop)
        return NAC;

    if (strcmp(token, "login") == 0
        && logged == true
        && !unlockop)
        return SAO;

    if (strcmp(token, "transfer") == 0)
        transferop = true;

    return 1;
}

int main(int argc, char *argv[])
{
    char buffer[BUFLEN], reply[BUFLEN];
    int tcp_socket, udp_socket, result;
    struct sockaddr_in serv_addr;
    FILE *log_file = NULL;

    lastcard = -1;

    char filename[BUFLEN];
    sprintf(filename, "client-%d.log", getpid());

    fd_set read_fds, tmp_fds;
    int fdmax;

    if (argc < 3)
    {
       fprintf(stderr, "CLIENT> Usage client: %s <IP_server> <port_server>\n", argv[0]);
       exit(0);
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error(FERR, log_file, "socket()");
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        error(FERR, log_file, "socket()");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);

    if ( (log_file = fopen(filename, "w")) == NULL)
		error(FERR, log_file, "fopen()");

    if (connect(tcp_socket, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
       error(FERR, log_file, "connect()");

    FD_SET(0, &read_fds);
    FD_SET(tcp_socket, &read_fds);
    fdmax = tcp_socket;

    FD_SET(udp_socket, &read_fds);
	if (fdmax < udp_socket)
        fdmax = udp_socket;

    while (true)
    {
        tmp_fds = read_fds;

        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error(FERR, log_file, "select()");

		if (FD_ISSET(0, &tmp_fds))
        {
            memset(buffer, 0 , BUFLEN);
            fgets(buffer, BUFLEN - 1, stdin);

            result = mastercommands(buffer);

            if (result < 0)
            {
                ibank_error_t err_code = (ibank_error_t) result;
                error(err_code, log_file, "");
            }

            if (result == 2)
            {
                result = sendto(udp_socket, buffer, BUFLEN, 0, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr));
                if (result < 0)
                    error(FERR, log_file, "sendto()");
            }
            else if (result == 1)
            {
                result = send(tcp_socket, buffer, BUFLEN, 0);
                if (result < 0)
                    error(FERR, log_file, "send()");
            }
            if (strstr(buffer, "unlock") != NULL)
            {
                memset(buffer, 0 , BUFLEN);
                sprintf(buffer, "unlock\n");
            }
        }

        if (FD_ISSET(tcp_socket, &tmp_fds))
        {
            memset(reply, 0 , BUFLEN);
            result = recv(tcp_socket, reply, BUFLEN, 0);

            if (result <= 0)
            {
                if (result == 0)
                {
                    printf("CLIENT> Server is closed\n");
                    clean_exit(tcp_socket, udp_socket, log_file);
                    return 0;
                }
                else
                {
                    error(FERR, log_file, "recv()");
                }
            }

            char cpy[BUFLEN];
            strcpy(cpy, reply);

            char *token;
            token = strtok(cpy, " \n");
            if (strcmp(token, "IBANK>") == 0)
            {
                token = strtok(NULL," \n");

                if (strcmp(token, "Welcome") == 0)
                    logged = true;
                if (strcmp(reply, "IBANK> Client disconnected\n") == 0)
                    logged = false;
                if (strcmp(reply, "IBANK> Transfer made\n") == 0
                    && transferop)
                    transferop = false;
                if (strcmp(reply, "IBANK> -9 : Operation failed\n") == 0
                    && transferop)
                    transferop = false;
                 if (strcmp(reply, "IBANK> The server will go offline\n") == 0
                    && transferop)
                    exitflag = true;
            }

            printf("%s", reply);
            if (strcmp(reply, "") != 0)
            {
                char cpy[BUFLEN];
                char *header;
                strcpy(cpy, reply);

                header = strtok(cpy, " \n");
                if (strcmp(header, "IBANK>") == 0)
                {
                    fprintf(log_file, "%s", buffer);
                    fprintf(log_file, "%s", reply);
                }
                if (strcmp(buffer, "quit\n") == 0)
                {
                    fprintf(log_file, "%s", buffer);
                }
            }
        }

        if (FD_ISSET(udp_socket, &tmp_fds))
        {
            memset(reply, 0 , BUFLEN);
            int size = sizeof(struct sockaddr);
            result = recvfrom(udp_socket, reply, BUFLEN, 0, (struct sockaddr*) &serv_addr, &size);

             if (result <= 0)
             {
                if (result == 0)
                {
                    printf("CLIENT> Server is closed\n");
                    exitflag = true;
                }
                else
                {
                    error(FERR, log_file, "recvfrom()");
                }
             }

            if (strcmp(reply, "UNLOCK> Card number unblocked\n") == 0
                && unlockop)
                unlockop = false;
            if (strcmp(reply, "UNLOCK> −7 : Unblocking failed\n") == 0
                && unlockop)
                unlockop = false;
            if (strcmp(reply, "UNLOCK> −6 : Operation failed\n") == 0
                && unlockop)
                unlockop = false;

            printf("%s", reply);
            if (strcmp(reply, "") != 0)
            {
                char cpy[BUFLEN];
                char *header;
                strcpy(cpy, reply);

                header = strtok (cpy, " \n");
                if (strcmp(header, "UNLOCK>") == 0)
                {
                    fprintf(log_file, "%s", buffer);
                    fprintf(log_file, "%s", reply);
                }
                if (strcmp(buffer, "quit\n") == 0)
                {
                    fprintf(log_file, "%s", buffer);
                }
            }
        }
        
        if (exitflag)
            break;
    }

    clean_exit(tcp_socket, udp_socket, log_file);
    return 0;
}
