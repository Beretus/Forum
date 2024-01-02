#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1024
#define USER_FILE "users.txt"

struct user_info {
    char username[50];
    struct sockaddr_in address;
    socklen_t addr_len;
    int active;
    int logged_in;
    time_t last_heartbeat;
};

struct user_info clients[MAX_CLIENTS];

int find_client(struct sockaddr_in *cli_addr) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && memcmp(cli_addr, &clients[i].address, sizeof(struct sockaddr_in)) == 0) {
            return i;
        }
    }
    return -1;
}

int check_existing_user(struct user_info users[], int num_users, char *new_username) {
    for (int i = 0; i < num_users; i++) {
        if (users[i].logged_in == 1 && strcmp(users[i].username, new_username) == 0) {
            return 1;
        }
    }
    return 0;
}

int add_client(struct sockaddr_in *cli_addr, socklen_t addr_len, char *username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            strncpy(clients[i].username, username, sizeof(clients[i].username) - 1);
            clients[i].address = *cli_addr;
            clients[i].addr_len = addr_len;
            clients[i].active = 1;
            clients[i].logged_in = 0;
            return i;
        }
    }
    return -1;
}


void broadcast_message(int sockfd, char *username, char *message, struct user_info *clients, int sender_index) {
    char full_message[BUFFER_SIZE];
    sprintf(full_message, "%s: %s", username, message);
    printf("%s\n", full_message);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].logged_in && i != sender_index) {
            sendto(sockfd, full_message, strlen(full_message), 0, (const struct sockaddr *)&clients[i].address, clients[i].addr_len);
        }
    }
}

void broadcast_log(int sockfd, char *message, struct user_info *clients, int sender_index) {
    char full_message[BUFFER_SIZE];
    sprintf(full_message, "%s", message);
    printf("%s\n", full_message);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].logged_in && i != sender_index) {
            sendto(sockfd, full_message, strlen(full_message), 0, (const struct sockaddr *)&clients[i].address, clients[i].addr_len);
        }
    }
}

int check_credentials(char *username, char *password) {
    FILE *file = fopen(USER_FILE, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 0;
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        char *file_username = strtok(line, ":");
        char *file_password = strtok(NULL, "\n");
        if (strcmp(username, file_username) == 0 && strcmp(password, file_password) == 0) {
            fclose(file);
            syslog(LOG_LOCAL1 | LOG_INFO, "Udana proba logowania: %s", username);
            return 1; //login udany
        }
    }
    fclose(file);
    syslog(LOG_LOCAL1 | LOG_ERR, "Nieudana proba logowania: %s", username);
    return 0; // login nieudany
}

int register_user(char *username, char *password) {
    FILE *file = fopen(USER_FILE, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 0;
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        char *file_username = strtok(line, ":");
        if (strcmp(username, file_username) == 0) {
            fclose(file);
            syslog(LOG_LOCAL1 | LOG_ERR, "Proba rejestracji istniejacego uzytkownika! %s", username);
            return 0; // user istnieje
        }
    }
    fclose(file);

    file = fopen(USER_FILE, "a");
    if (file == NULL) {
        perror("Error opening file");
        return 0;
    }
    fprintf(file, "%s:%s\n", username, password);
    fclose(file);
    syslog(LOG_LOCAL1 | LOG_INFO, "Uzytkownik %s zarejestrowany", username);
    return 1; // rejestracja udana
}

void handle_login(int sockfd, struct sockaddr_in *cli_addr, socklen_t addr_len, char *username, char *password) {
    if (check_credentials(username, password)) {
        int client_index = find_client(cli_addr);
        if (client_index == -1) {
            client_index = add_client(cli_addr, addr_len, username);
        }
        if (client_index != -1) {
            int user_exists = check_existing_user(clients, 3, username);
            if(user_exists) {
                char response[] = "Uzytkownik o takiej nazwie jest juz zalogowany";
            sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)cli_addr, addr_len); 
            
            } else {
               clients[client_index].logged_in = 1;
	       clients[client_index].last_heartbeat = time(NULL);
            char response[] = "LOGIN_SUCCESS";
            sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)cli_addr, addr_len);
            }
            
        }
    } else {
        char response[] = "Niepoprawna proba logowania!";
        sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)cli_addr, addr_len);
    }
}

void handle_register(int sockfd, struct sockaddr_in *cli_addr, socklen_t addr_len, char *username, char *password) {
    if (register_user(username, password)) {
        int client_index = add_client(cli_addr, addr_len, username);
        if (client_index != -1) {
            clients[client_index].logged_in = 1;
	    clients[client_index].last_heartbeat = time(NULL);
            char response[] = "REGISTER_SUCCESS";
            syslog(LOG_LOCAL1 | LOG_INFO, "Udana rejestracja %s", username);
            sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)cli_addr, addr_len);

        }
    } else {
        char response[] = "Rejestracja nieudana!";
        syslog(LOG_LOCAL1 | LOG_ERR, "Nieudana rejestracja %s", username);
        sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)cli_addr, addr_len);
    }
}

int main() {
    openlog("server3", LOG_PID, LOG_DAEMON);
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    fd_set readfds;
    int max_sd, activity, valread;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Serwer Forum wlaczony, nasluchiwanie na porcie %d\n", PORT);
    max_sd = sockfd;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);
        if (activity > 0 && FD_ISSET(sockfd, &readfds)) {
            char buffer[BUFFER_SIZE];
            socklen_t len = sizeof(cliaddr);
            valread = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr *)&cliaddr, &len);

            if (valread > 0) {
                buffer[valread] = '\0';
                char *action = strtok(buffer, ":");
                char *username = strtok(NULL, ":");
                char *password = strtok(NULL, "\n");

                if (strcmp(action, "LOGIN") == 0) {
                    handle_login(sockfd, &cliaddr, len, username, password);
		            int client_index = find_client(&cliaddr);
                    if (client_index != -1 && clients[client_index].logged_in) {
                        char annoucement[BUFFER_SIZE];
                        sprintf(annoucement, "Uzytkownik %s dolaczyl do forum!", username);
                        syslog(LOG_LOCAL1 | LOG_INFO, "UWAGA: %s", annoucement);
                        broadcast_log(sockfd, annoucement, clients, client_index);
                        }
                } else if (strcmp(action, "REGISTER") == 0) {

                    handle_register(sockfd, &cliaddr, len, username, password);
		            int client_index = find_client(&cliaddr);
                    if (client_index != -1 && clients[client_index].logged_in) {
                        char annoucement[BUFFER_SIZE];
                        sprintf(annoucement, "Uzytkownik %s zarejestrowal sie do forum!", username);
                        syslog(LOG_LOCAL1 | LOG_INFO, "UWAGA: %s", annoucement);
                        broadcast_log(sockfd, annoucement, clients, client_index);
                    }
                }else if (strcmp(action, "EXIT") == 0) {
                    int client_index = find_client(&cliaddr);
                    if (client_index != -1 && clients[client_index].logged_in) {
                        char annoucement[BUFFER_SIZE];
                        sprintf(annoucement, "Uzytkownik %s opuscil forum!", username);
                        syslog(LOG_LOCAL1 | LOG_INFO, "UWAGA: %s", annoucement);
                        broadcast_log(sockfd, annoucement, clients, client_index);
                        clients[client_index].active = 0;
                        clients[client_index].logged_in = 0;
                        strcpy(clients[client_index].username, "");
                    }
                }
		else if (strcmp(action, "HEARTBEAT") == 0) {
                int client_index = find_client(&cliaddr);
                if (client_index != -1) {
                    clients[client_index].last_heartbeat = time(NULL);
                    char *message = "HEARTBEAT_ACK";
                    sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
		            // printf("Otrzymano HEARTBEAT od klienta: %s\n", clients[client_index].username);
                }
		}
                else {
			        int client_index = find_client(&cliaddr);
                    if (client_index != -1 && clients[client_index].logged_in) {
                    broadcast_message(sockfd, clients[client_index].username, password, clients, client_index);
		            }   
            }
        }
    }
	for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].logged_in && time(NULL) - clients[i].last_heartbeat > 8) {
            printf("Klient %s stracil polaczenie\n", clients[i].username);
            syslog(LOG_LOCAL1 | LOG_INFO, "Klient %s utracil polaczenie.", clients[i].username);
            clients[i].active = 0;
            strcpy(clients[i].username, "");
        }
    }
}
    closelog();
    return 0;
}

