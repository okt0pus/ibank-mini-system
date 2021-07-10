#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS	5
#define BUFLEN 256


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

typedef struct
{
	char lname[13];
	char fname[13];
	char password[9];
	int no_card;
	int pin;
	double sold;
	bool blocked;
	bool authenticated;
	int card_unlock;
} user_t;

typedef struct
{
	user_t *user_to;
	double value;
} transfer_t;

typedef struct
{
	int socket;
	char last_command[BUFLEN];
	int count;
	transfer_t transfer;
	user_t *user;
} open_client_t;

open_client_t clients[MAX_CLIENTS];
char buffer[BUFLEN], reply[BUFLEN];
int exitflag;

void error(ibank_error_t code, char *msg)
{
	switch (code)
    {
    case NAC:
        printf("%d : The client is not authenticated\n", NAC);
        break;
    case SAO:
        printf("%d : Session already opened\n", SAO);
        break;
    case WPIN:
        printf("%d : Wrong PIN\n", WPIN);
        break;
    case CNF:
        printf("%d : Card number doesn't exist\n", CNF);
        break;
    case CBK:
        printf("%d : Card number blocked\n", CBK);
        break;
    case OPF:
        printf("%d : Operation failed\n", OPF);
        break;
    case UBF:
        printf("%d : Unblocking failed\n", UBF);
        break;
    case IFU:
        printf("%d : Insuficient funds\n", IFU);
        break;
    case OPC:
        printf("%d : Operation canceled\n", OPC);
        break;
    case FERR:
        printf("%d : Function error: %s\n", FERR, msg);
        exit(1);
        break;
    default:
        break;
	}
}

void update_clients(int socket, char* last_command, int contor, char* msg)
{
	if (strcmp(msg, "clear") == 0)
	{
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			clients[i].socket = -1;
			clients[i].user = NULL;
			clients[i].count = 0;
			memset(clients[i].last_command, 0, BUFLEN);
			clients[i].transfer.user_to = NULL;
			clients[i].transfer.value = 0.0;
		}
	}
	if (strcmp(msg, "delete") == 0)
	{
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (clients[i].socket == socket)
			{
				clients[i].socket = -1;
				clients[i].user = NULL;
				clients[i].count = 0;
				memset(clients[i].last_command, 0, BUFLEN);
				clients[i].transfer.user_to = NULL;
				clients[i].transfer.value = 0.0;
				break;
			}
		}
	}
	if (strcmp(msg, "initialize") == 0)
	{
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (clients[i].socket == -1)
			{
				clients[i].socket = socket;
				clients[i].user = NULL;
				clients[i].count = 0;
				memset(clients[i].last_command, 0, BUFLEN);
				clients[i].transfer.user_to = NULL;
				clients[i].transfer.value = 0.0;
				break;
			}
		}
	}
}

void login(int socket, user_t* users, int len)
{
	char *token;

	char cpy[BUFLEN];
    strcpy(cpy, reply);

    token = strtok (cpy, " \n");
	token = strtok (NULL, " \n");

	if (token == NULL)
	{
		strcpy(buffer, "login command incomplete\n");
		return;
	}

	int no_card = atoi(token);

	token = strtok(NULL, " \n");
	if (token == NULL)
	{
		strcpy(buffer, "login command incomplete\n");
		return;
	}

	int pin = atoi(token);

	int i_u = -1, i_c = -1;
	for(int i = 0; i < len; ++i)
	{
		if (users[i].no_card == no_card)
		{
			i_u = i;
			break;
		}
	}
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i].socket == socket)
		{
			i_c = i;
			break;
		}
	}
	if (i_c == -1)
	{
		return;
	}

	memset(clients[i_c].last_command, 0, BUFLEN);
	strcpy(clients[i_c].last_command, reply);
	clients[i_c].transfer.user_to = NULL;
	clients[i_c].transfer.value = 0.0;


	if (i_u == -1)
	{
		strcpy(buffer, "IBANK> -4 : Card number doesn't exist\n");
		clients[i_c].count = 0;
		clients[i_c].user = NULL;
		return;
	}

	bool flag = false;
	if (users[i_u].blocked == true)
	{
		strcpy(buffer, "IBANK> -5 : Card number blocked\n");
		clients[i_c].user = NULL;
		return;
	}

	if (users[i_u].authenticated == true)
	{
		strcpy(buffer, "IBANK> -2 : Session already opened\n");
		clients[i_c].count = 0;
		clients[i_c].user = NULL;
		return;
	}

	if (pin != users[i_u].pin)
	{
		if (strcmp(clients[i_c].last_command, reply) == 0
			|| strcmp(clients[i_c].last_command, "") == 0)
		{
			clients[i_c].count++;
			if (clients[i_c].count == 3)
			{
				strcpy(buffer, "IBANK> -5 : Card number blocked\n");
				users[i_u].blocked = true;
			}
			else
			{
				strcpy(buffer, "IBANK> -3 : Wrong PIN\n");
			}
		}
		else
		{
			clients[i_c].count = 1;
			strcpy(buffer, "IBANK> -3 : Wrong PIN\n");
		}
		clients[i_c].user = NULL;
		return;
	}

	clients[i_c].count = 0;
	users[i_u].authenticated = true;
	clients[i_c].user = &users[i_u];
	sprintf(buffer, "IBANK> Welcome %s %s\n", clients[i_c].user->lname, clients[i_c].user->fname);
}

void logout(int socket, user_t *users, int len)
{
	int i_u = -1, i_c = -1;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i].socket == socket)
		{
			i_c = i;
			break;
		}
	}

	for(int i = 0; i < len; ++i)
	{
		if (users[i].no_card == clients[i_c].user->no_card)
		{
			i_u = i;
			break;
		}
	}

	clients[i_c].count = 0;
	memset(clients[i_c].last_command, 0, BUFLEN);
	strcpy(clients[i_c].last_command, reply);
	clients[i_c].transfer.user_to = NULL;
	clients[i_c].transfer.value = 0.0;
	clients[i_c].user = NULL;
	users[i_c].authenticated = false;

	strcat(buffer,"IBANK> The client is not authenticated\n");
}

void listsold(int socket, user_t *users, int len)
{
	int i_u = -1, i_c = -1;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if (clients[i].socket == socket)
		{
			i_c = i;
			break;
		}

	for(int i = 0; i < len; ++i)
		if (users[i].no_card == clients[i_c].user->no_card)
		{
			i_u = i;
			break;
		}

	memset(clients[i_c].last_command, 0, BUFLEN);
	strcpy(clients[i_c].last_command, reply);
	clients[i_c].user = &users[i_c];

	sprintf(buffer, "IBANK> %.2f\n", clients[i_c].user->sold);
}

void transfer(int socket, user_t *users, int len)
{
	char *token;

	char cpy[BUFLEN];
    strcpy(cpy,reply);

    token = strtok(cpy, " \n");
	token = strtok(NULL, " \n");
	if (token == NULL)
	{
		strcpy(buffer, "Transfer command incomplete\n");
		return;
	}

	int no_card = atoi(token);

	token = strtok(NULL, " \n");
	if (token == NULL)
	{
		strcpy(buffer, "Transfer command incomplete\n");
		return;
	}

	double value = atof(token);

	int i_u = -1, i_c = -1;
	for(int i = 0; i < len; ++i)
		if (users[i].no_card == no_card)
		{
			i_u = i;
			break;
		}
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if (clients[i].socket == socket)
		{
			i_c = i;
			break;
		}

	memset(clients[i_c].last_command, 0, BUFLEN);
	strcpy(clients[i_c].last_command, reply);

	if (i_u == -1)
	{
		strcpy(buffer, "IBANK> -4 : Card number doesn't exist\n");
		return;
	}

	if (clients[i_c].user->sold < value)
	{
		strcpy(buffer, "IBANK> -8 : Insuficient funds\n");
		clients[i_c].user = NULL;
		return;
	}

	clients[i_c].transfer.user_to = &users[i_u];
	clients[i_c].transfer.value = value;
	sprintf(buffer, "IBANK> Transfer %.2f to %s %s ? [y /n]\n", value,
					clients[i_c].transfer.user_to->lname,
					clients[i_c].transfer.user_to->fname);
}

void administrationtransfer(int sock, user_t *users, int len)
{
	int i_u = -1, i_c = -1;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if (clients[i].socket == sock)
		{
			i_c = i;
			break;
		}
	for(int i = 0; i < len; ++i)
		if (users[i].no_card == clients[i_c].transfer.user_to->no_card)
		{
			i_u = i;
			break;
		}


	if (reply[0] != 'y')
	{
		strcpy(buffer, "IBANK> -9 : Operation canceled\n");
		clients[i_c].transfer.user_to = NULL;
		clients[i_c].transfer.value = 0.0;
	}
	else
	{
		users[i_u].sold += clients[i_c].transfer.value;

		i_u = -1;
		for(int i = 0; i < len; ++i)
			if (users[i].no_card == clients[i_c].user->no_card)
			{
				i_u = i;
				break;
			}

		users[i_u].sold -= clients[i_c].transfer.value;
		clients[i_c].transfer.user_to = NULL;
		clients[i_c].transfer.value = 0.0;

		strcpy(buffer, "IBANK> Transfer made\n");
	}
}

void unclock(user_t *users, int len)
{
	char *token;

	char cpy[BUFLEN];
    strcpy(cpy, reply);

    token = strtok(cpy, " \n");
	token = strtok(NULL, " \n");
	if (token == NULL)
	{
		strcpy(buffer,"Unlock command incomplete\n");
		return;
	}

	int no_card = atoi(token);

	int i_u = -1;
	for(int i = 0; i < len; ++i)
		if (users[i].no_card == no_card)
		{
			i_u = i;
			break;
		}

	if (i_u == -1)
	{
		strcpy(buffer, "UNLOCK> -4 : Card number doesn't exist\n");
		return;
	}

	if (users[i_u].blocked == false)
	{
		strcpy(buffer, "UNLOCK> −6 : Operation failed\n");
		return;
	}

	for(int i = 0; i < len; ++i)
	{
		if (users[i].card_unlock == no_card)
		{
			strcpy(buffer, "UNLOCK> −7 : Unlocking failed\n");
			return;
		}
	}

	users[i_u].card_unlock = no_card;
	sprintf(buffer, "UNLOCK> Send secret password\n");
}

void administrationunlock(user_t *users, int len)
{
	char *token;

	char cpy[BUFLEN];
    strcpy(cpy,reply);

    token = strtok(cpy, " \n");
	if (token == NULL)
	{
		strcpy(buffer, "Unlocking command incomplete\n");
		return;
	}

	int no_card = atoi(token);

	token = strtok(NULL, " \n");
	if (token == NULL)
	{
		strcpy(buffer, "Unlocking command incomplete\n");
		return;
	}

	char password[9];
	strcpy(password, token);

	int i_u = -1;
	for(int i = 0; i < len; ++i)
	{
		if (users[i].no_card == no_card)
		{
			i_u = i;
			break;
		}
	}

	if (i_u == -1)
	{
		strcpy(buffer, "UNLOCK> -4 : Card number doesn't exist\n");
		return;
	}

	if (strcmp(users[i_u].password, password) != 0)
	{
		users[i_u].card_unlock = 0;
		strcpy(buffer, "UNLOCK> −7 : Unlocking failed\n");
		return;
	}

	users[i_u].card_unlock = 0;
	users[i_u].blocked = false;
	sprintf(buffer, "UNLOCK> Card unblocked\n");
}

void mastercommands(int socket, user_t *users, int len)
{
	char cpy[BUFLEN];
    strcpy(cpy, reply);

    char *option;
    option = strtok(cpy, " \n");

	memset(buffer, 0, BUFLEN);

	int client = -1;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i].socket == socket)
		{
			client = i;
			break;
		}
	}
	if (client == -1)
	{
		return;
	}

    if (strcmp(option, "login") == 0)
	{
		login(socket, users, len);
		return;
	}

	if (strcmp(option, "logout") == 0)
	{
		logout(socket, users, len);
		return;
	}

	if (strcmp(option, "listsold") == 0)
	{
		listsold(socket, users, len);
		return;
	}

	if (strcmp(option, "transfer") == 0)
	{
		transfer(socket, users, len);
		return;
	}

	if (strcmp(option, "unlock") == 0)
	{
		unclock(users, len);
		return;
	}

	if (clients[client].transfer.value != 0)
		administrationtransfer(socket, users, len);

	if (isdigit(reply[0]))
		administrationunlock(users, len);
}

void read_info(FILE* database, user_t *users, int len)
{
	char buffer[BUFLEN];
	int count = 0;

	while (fgets(buffer, BUFLEN, database) != NULL)
	{
		char *token;
    	char delimiters[2] = " ";

    	token = strtok(buffer, delimiters);
		if (token == NULL)
			error(FERR, "strtok()");
		strcpy(users[count].lname, token);

		token = strtok(NULL, delimiters);
		if (token == NULL)
			error(FERR, "strtok()");
		strcpy(users[count].fname, token);

		token = strtok(NULL, delimiters);
		if (token == NULL)
			error(FERR, "strtok()");
		users[count].no_card = atoi(token);

		token = strtok(NULL, delimiters);
		if (token == NULL)
			error(FERR, "strtok()");
		users[count].pin = atoi(token);

		token = strtok(NULL, delimiters);
		if (token == NULL)
			error(FERR, "strtok()");
		strcpy(users[count].password, token);

		token = strtok(NULL, delimiters);
		if (token == NULL)
			error(FERR, "strtok()");
		users[count].sold = atof(token);

		users[count].blocked = false;
		users[count].authenticated = false;
		users[count].card_unlock = 0;
		count++;
	}

	if (count != len)
		error(FERR, "fgets()");

	for(int i = 0; i < len; ++i)
	{
		printf("This is user %d ::\n", i);
		printf("%s\n", users[i].lname);
		printf("%s\n", users[i].fname);
		printf("%s\n", users[i].password);
		printf("%d\n", users[i].no_card);
		printf("%d\n", users[i].pin);
		printf("%f\n", users[i].sold);
		printf("%d\n", users[i].blocked);
		printf("%d\n", users[i].authenticated);
		printf("%d\n", users[i].card_unlock);
	}

}

int main(int argc, char *argv[])
{
	FILE *users_data_file;
	int tcp_socket, udp_socket, new_socket_connect, port_no, client_len;

	struct sockaddr_in serv_addr, client_addr;

	fd_set read_fds, tmp_fds;
	int fdmax;
	int result;

	if (argc < 3)
	{
		fprintf(stderr, "Usage server: %s <port_server> <users_data_file>\n", argv[0]);
		exit(1);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		error(FERR, "socket()");

	if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		error(FERR, "socket()");


	if ((users_data_file = fopen(argv[2], "r")) == NULL)
		error(FERR, "fopen()");

	memset(buffer, 0, BUFLEN);
	if (fgets(buffer, BUFLEN, users_data_file) == NULL)
		error(FERR, "fgets()");

	int max_users = atoi(buffer);

	user_t utilizatori[max_users];
	read_info(users_data_file, utilizatori, max_users);

	if (fclose(users_data_file) == EOF)
		error(FERR, "fclose()");

	port_no = atoi(argv[1]);

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port_no);

	if (bind(tcp_socket, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
			error(FERR, "bind()");

	if (bind(udp_socket, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
			error(FERR, "bind()");

	listen(tcp_socket, MAX_CLIENTS);

	FD_SET(0, &read_fds);
	FD_SET(tcp_socket, &read_fds);
	fdmax = tcp_socket;

	FD_SET(udp_socket, &read_fds);
	if (fdmax < udp_socket)
		fdmax = udp_socket;

	update_clients(0, "", 0, "clear");
	while (true)
	{
		tmp_fds = read_fds;

		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error(FERR, "select()");

		for(int sock = 0; sock <= fdmax; ++sock)
		{

			if (FD_ISSET(sock, &tmp_fds))
			{
				if (sock == 0)
				{
					memset(buffer, 0 , BUFLEN);
            		fgets(buffer, BUFLEN - 1, stdin);

					if (strcmp(buffer, "quit\n")  == 0)
					{
						memset(buffer, 0 , BUFLEN);
						sprintf(buffer, "IBANK> The server will go offline\n");

						for(int s = 1; s <= fdmax; ++s)
							if (FD_ISSET(s, &read_fds))
							{
								if (s != tcp_socket && s != udp_socket)
								{
									result = send(s, buffer, BUFLEN, 0);
                    				if (result < 0)
										error(FERR, "send()");
								}
							}
						exitflag = true;
					}
				}
				else
				{
					if (sock == tcp_socket)
					{
						client_len = sizeof(struct sockaddr_in);

						if ((new_socket_connect = accept(tcp_socket, (struct sockaddr *) &client_addr, &client_len)) == -1)
						{
							error(FERR, "accept()");
						}
						else
						{
							FD_SET(new_socket_connect, &read_fds);
							if (new_socket_connect > fdmax)
								fdmax = new_socket_connect;
						}
						update_clients(new_socket_connect, "", 0, "initialize");
						printf("New connection from [IP -> %s]   [PORT -> %d]   [SOCKET -> %d]\n",
									inet_ntoa(client_addr.sin_addr),
									ntohs(client_addr.sin_port),
									new_socket_connect);
					}
					else
					{
						if (sock == udp_socket)
						{
							memset(reply, 0 , BUFLEN);
            				int size = sizeof(struct sockaddr);
							result = recvfrom(udp_socket, reply, BUFLEN, 0, (struct sockaddr*) &serv_addr, &size);
            				if (result < 0)
								error(FERR, "recvfrom()");

							printf ("(UDP) Client sent message: %s", reply);
							mastercommands(sock, utilizatori, max_users);

							result = sendto(udp_socket, buffer, BUFLEN, 0, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr));
                			if (result < 0)
								error(FERR, "sendto()");

						}
						else
						{
							memset(reply, 0, BUFLEN);
							result = recv(sock, reply, BUFLEN, 0);

							if (result <= 0)
							{
								if (result == 0)
								{
									update_clients(sock, "", 0, "delete");
									printf("SERVER> The client connected on %d leaves\n", sock);
								}
								else
								{
									error(FERR, "recv()");
								}

								if (close(sock) < 0)
									error(FERR, "close()");
								FD_CLR(sock, &read_fds);
							}
							else
							{
								printf ("(TCP) The client connected on %d, sent the command: %s", sock, reply);

								if (strcmp(reply,"quit\n") == 0)
								{
									printf ("CLIENT> The client connected on  %d will leave soon\n", sock);
								}

								mastercommands(sock, utilizatori, max_users);

								result = send(sock, buffer, BUFLEN, 0);
								if (result < 0)
									error(FERR, "send()");
							}
						}
					}
				}
			}
		}

		if (exitflag)
			break;
	}

	if (close(tcp_socket) < 0)
		error(FERR, "close()");
	if (close(udp_socket) < 0)
		error(FERR, "close()");
	return 0;
}
