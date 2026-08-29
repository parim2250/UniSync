#ifndef NETWORK_H
#define NETWORK_H

#define DEFAULT_PORT   9876
#define BUFFER_SIZE    1024
#define BACKLOG        5

int create_tcp_socket(void);
int start_server(int port);
int connect_to_server(const char *ip, int port);
int send_message(int sockfd, const char *msg);
int receive_message(int sockfd, char *buffer, int buf_size);
void close_socket(int sockfd);

#endif /* NETWORK_H */
