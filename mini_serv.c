#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/select.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>

typedef struct  s_client{
    int         id;
    char*       buffer;
}               t_client;

void    ft_error(char* str);
char *str_join(char *buf, char *add);
int extract_message(char **buf, char **msg);


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        ft_error("Wrong number of arguments\n");
        return (1);
    }

    int server_fd;
    struct sockaddr_in  server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (server_fd == -1)
    {
        ft_error("Fatal error\n");
        return (2);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(2130706433);
    server_addr.sin_port = htons(atoi(argv[1]));
    
    if (bind(server_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        ft_error("Fatal error\n");
        return (3);
    }
    
    if (listen(server_fd, 32) != 0)
    {
        ft_error("Fatal error\n");
        return (4);
    }

    t_client    clients[FD_SETSIZE];
    memset(&clients, 0, sizeof(clients));
    
    int     maxfd = server_fd;
    int     id_available = 0;

    fd_set  total, rd, rw;

    FD_ZERO(&total);
    FD_SET(server_fd, &total);

    while(1)
    {
        rd = total;
        rw = total;

        if (select(maxfd + 1, &rd, &rw, NULL, NULL) == -1)
            continue;
        
        for (int fd = server_fd; fd < maxfd + 1; fd++)
        {
            //first connection
            if (fd == server_fd && FD_ISSET(fd, &rd))
            {
                struct sockaddr client_adr;
                memset(&client_adr, 0, sizeof(client_adr));

                unsigned int    len = sizeof(client_adr);
                
                int client_fd = accept(fd, (struct sockaddr *)&client_adr, (socklen_t *)&len);

                if (client_fd == -1)
                    continue;
                
                FD_SET(client_fd, &total);
                
                if (client_fd > maxfd)
                    maxfd = client_fd;
                
                clients[client_fd].id = id_available;
                id_available += 1;
                
                if (clients[client_fd].buffer != NULL)
                {
                    free(clients[client_fd].buffer);
                    clients[client_fd].buffer = NULL;
                }

                //send message

                char    buf[100];
                memset(&buf, 0, sizeof(buf));
                
                sprintf(buf, "server: client %d just arrived\n", clients[client_fd].id);

                for (int i = server_fd + 1; i < maxfd + 1; i++)
                {
                    if (i != client_fd && FD_ISSET(i, &rw) && clients[client_fd].id != -1)
                    {
                        send(i, buf, strlen(buf), MSG_DONTWAIT);
                    }
                }
                
            }

            else if (FD_ISSET(fd, &rd))
            {
                char    rec_msg[120000];
                memset(&rec_msg, 0, sizeof(rec_msg));

                size_t  bytes = recv(fd, rec_msg, sizeof(rec_msg), MSG_DONTWAIT);

                if (bytes == 0)
                {
                    //connection closed

                    close(fd);
                    if (clients[fd].buffer != NULL)
                    {
                        free(clients[fd].buffer);
                        clients[fd].buffer = NULL;
                    }
                    FD_CLR(fd, &total);
                    
                    //send message

                    char    buf[100];
                    memset(&buf, 0, sizeof(buf));
                    
                    sprintf(buf, "server: client %d just left\n", clients[fd].id);

                    for (int i = server_fd + 1; i < maxfd + 1; i++)
                    {
                        if (i != fd && FD_ISSET(i, &rw) && clients[fd].id != -1)
                        {
                            send(i, buf, strlen(buf), MSG_DONTWAIT);
                        }
                    }

                    clients[fd].id = -1;
                }
                else if (bytes > 0)
                {
                    clients[fd].buffer = str_join(clients[fd].buffer, rec_msg);
                    
                    char*   message_to_send;
                    
                    int error = extract_message(&clients[fd].buffer, &message_to_send);
                    
                    while (error == 1)
                    {
                        char    tmp[120100];
                        memset(tmp, 0, sizeof(tmp));
                        sprintf(tmp, "client %d: %s", clients[fd].id, message_to_send);

                        for (int i = server_fd + 1; i < maxfd + 1; i++)
                        {
                            if (i != fd && FD_ISSET(i, &rw) && clients[fd].id != -1)
                            {
                                send(i, tmp, strlen(tmp), MSG_DONTWAIT);
                            }
                        }

                        free(message_to_send);
                        error = extract_message(&clients[fd].buffer, &message_to_send);
                    }

                    if (error == -1)
                    {
                        for (int i = server_fd; i < maxfd + 1; i++)
                        {
                            close(i);
                            if (clients[i].buffer != NULL)
                                free(clients[i].buffer);
                            exit(10);
                        }
                    }
                }
            }
        }
    }
    
}

void    ft_error(char* str)
{
    if (str != NULL)
    {
        write(2, str, strlen(str));
    }
}

int extract_message(char **buf, char **msg)
{
    char    *newbuf;
    int i;

    *msg = 0;
    if (*buf == 0)
        return (0);
    i = 0;
    while ((*buf)[i])
    {
        if ((*buf)[i] == '\n')
        {
            newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
            if (newbuf == 0)
                return (-1);
            strcpy(newbuf, *buf + i + 1);
            *msg = *buf;
            (*msg)[i + 1] = 0;
            *buf = newbuf;
            return (1);
        }
        i++;
    }
    return (0);
}

char *str_join(char *buf, char *add)
{
    char    *newbuf;
    int     len;

    if (buf == 0)
        len = 0;
    else
        len = strlen(buf);
    newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
    if (newbuf == 0)
        return (0);
    newbuf[0] = 0;
    if (buf != 0)
        strcat(newbuf, buf);
    free(buf);
    strcat(newbuf, add);
    return (newbuf);
}