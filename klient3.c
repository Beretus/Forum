#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h> 
#include <sys/time.h>
#include <errno.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int sockfd;
struct sockaddr_in servaddr;
char username[50];
volatile sig_atomic_t sigflag = 0;

void send_request(int sockfd, struct sockaddr_in servaddr, char* username, char* password, char* action);
void handle_sigint(int sig);
void send_message(int sockfd, struct sockaddr_in servaddr, char* username, char* message, char* action);

void handle_sigint(int sig) {
    sigflag = 1;
}

void send_request(int sockfd, struct sockaddr_in servaddr, char* username, char* password, char* action) {
    char full_message[BUFFER_SIZE];
    sprintf(full_message, "%s:%s:%s", action, username, password);
    sendto(sockfd, (const char*)full_message, strlen(full_message), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr));
}

void send_message(int sockfd, struct sockaddr_in servaddr, char* username, char* message, char* action) {
    char full_message[BUFFER_SIZE];
    sprintf(full_message, "%s:%s:%s", action, username, message);
    int bytes_sent = sendto(sockfd, (const char*)full_message, strlen(full_message), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr));
    if (bytes_sent < 0) {
        perror("Blad wysylania wiadomosci");
    } else {
        //printf("Wysłano %d bajtów\n", bytes_sent);
    }
}

int main() {
    struct sockaddr_in servaddr;
    fd_set readfds;
    int max_sd, activity, valread;
    char buffer[BUFFER_SIZE];
    char password[50];
    int flags;
    int logged_in = 0;
    struct timeval last_heartbeat;
    time_t server_activity;
    gettimeofday(&last_heartbeat, NULL);
    server_activity = time(NULL);

    struct timeval now;


    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    max_sd = sockfd;

    do {
        printf("Podaj swoją nazwę użytkownika:");
        fgets(username, sizeof(username), stdin);
        username[strcspn(username, "\n")] = 0; 
        if(strlen(username) == 0) {
            printf("Niepoprawna nazwa użytkownika, nie może być pusta\n");
        }
    } while(strlen(username) == 0); 
    username[strcspn(username, "\n")] = 0;
    printf("EXIT - Wyjscie\nWybierz opcję:\n1. Logowanie\n2. Rejestracja\n3. EXIT\n");
    signal(SIGINT, handle_sigint);

    while (1) {
        if (sigflag) {
            char exit_message[BUFFER_SIZE] = "EXIT";
            send_message(sockfd, servaddr, username, exit_message, "EXIT");
            close(sockfd);
            exit(0);
        }

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if ((activity < 0) && (errno != EINTR)) {
            printf("select error");
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (!logged_in) {
                int choice;

                printf("Wybierz opcję:\n1. Logowanie\n2. Rejestracja\n3. EXIT\n");
                scanf("%d", &choice);
                getchar(); 

                if (choice == 3) {
                    exit(0); 
                } else if (choice == 1 || choice == 2) {
                    do {
                        printf("Podaj hasło:");
                        fgets(password, sizeof(password), stdin);
                        password[strcspn(password, "\n")] = 0; 
                        if(strlen(password) == 0) {
                            printf("**Niepoprawne hasło, nie może być puste**\n");
                        }
                    } while(strlen(password) == 0);

                    if (choice == 1) {
                        send_request(sockfd, servaddr, username, password, "LOGIN");
			            
                    } else if (choice == 2) {
                        send_request(sockfd, servaddr, username, password, "REGISTER");
                    }
                } else {
                    printf("**Nieprawidłowa opcja!**\n");
                }
            } else {
                
                fgets(buffer, BUFFER_SIZE, stdin);
                if(time(NULL) - server_activity > 7) {
                    printf("SERWER NIE ODPOWIADA! \n");
                }
                buffer[strcspn(buffer, "\n")] = 0; 

                if (strcmp(buffer, "EXIT") == 0) {
                    send_message(sockfd, servaddr, username, buffer, "EXIT");
                    exit(0);
                } else {
                    if (strlen(buffer) > 0) {
                        send_message(sockfd, servaddr, username, buffer, "MESSAGE");
                    }
                }
                
            }
        }
	struct timeval now;
	gettimeofday(&now, NULL);
	if (now.tv_sec - last_heartbeat.tv_sec >= 5) {
            send_message(sockfd, servaddr, username, "HEARTBEAT", "HEARTBEAT");
            last_heartbeat = now;
        }
            signal(SIGINT, handle_sigint);

       
        if (FD_ISSET(sockfd, &readfds)) {
            socklen_t len = sizeof(servaddr); 
            valread = recvfrom(sockfd, (char*)buffer, BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr*)&servaddr, &len);

            if (valread > 0) {
                buffer[valread] = '\0';
                printf("%s\n", buffer);
                if(strcmp(buffer, "HEARTBEAT_ACK") == 0) {
                    server_activity = time(NULL);
                 } else {
                    printf("%s\n", buffer);
                 }
                
                
                if (strcmp(buffer, "LOGIN_SUCCESS") == 0 || strcmp(buffer, "REGISTER_SUCCESS") == 0) {
                    logged_in = 1;
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
