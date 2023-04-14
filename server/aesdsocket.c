#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <linux/fs.h>

#define PORT "9000" // the port users will be connecting to
#define BACKLOG 10  // how many pending connections queue will hold
#define MAXBUFLEN 40000
#define OUTPUT_FILE_PATH "/var/tmp/aesdsocketdata"

static bool signaled = false;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

/* handler for SIGINT & SIGTERM */
static void signal_handler(int signo)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    signaled = true;
}

int main(int argc, char *argv[])
{
    int sockfd, acceptfd, outputfd, i;
    struct addrinfo hints, *servinfo;
    int status, numbytes, recvbytes;
    ssize_t ret;
    struct sockaddr_storage their_addr; // connector's address information
    char *buf;
    socklen_t addr_len;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    char *pbuf, *precvbuf, *nlinepos;
    pid_t pid;

    // register SIGINT & SIGTERM
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (-1 == sigaction(SIGINT, &sa, NULL))
    {
        fprintf(stderr, "Cannot handle SIGINT!\n");
        return -1;
    }
    if (-1 == sigaction(SIGTERM, &sa, NULL))
    {
        fprintf(stderr, "Cannot handle SIGTERM!\n");
        return -1;
    }

    // openlog for syslog
    openlog(NULL, 0, LOG_USER);

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    hints.ai_family = AF_UNSPEC;     // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0; /*0=any*/

    // getaddrinfo()
    if (0 != (status = getaddrinfo(/*node*/ NULL, /*service*/ PORT, /*hints*/ &hints, /*res*/ &servinfo)))
    {
        fprintf(stderr, "getaddrinfo failed with error:%s\n", gai_strerror(status));
        return -1;
    }

    // socket()
    if (-1 == (sockfd = socket(/*domain*/ servinfo->ai_family, /*type*/ servinfo->ai_socktype, /*protocol*/ servinfo->ai_protocol)))
    {
        perror("socket");
        return -1;
    }
    printf("server: socket created\n");

    // setsockopt for SO_REUSEADDR
    if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))
    {
        perror("setsockopt");
        return -1;
    }

    // bind()
    if (-1 == bind(/*sockfd*/ sockfd, /*addr*/ servinfo->ai_addr, /*addrlen*/ servinfo->ai_addrlen))
    {
        close(sockfd);
        perror("bind");
        return -1;
    }
    printf("server: socket binded\n");

    freeaddrinfo(servinfo); // free the structure

    if (2 == argc && (0 == strcmp("-d", argv[1])))
    {
        // create new process
        pid = fork();
        if (-1 == pid)
        {
            perror("fork");
            return -1;
        }
        // parent
        else if (pid > 0)
        {
            if (-1 == close(sockfd))
            {
                perror("close-sockfd-parent");
                return -1;
            }
            printf("server: closed sockfd from parent\n");

            closelog();
            return 0;
        }

        /* create new session and process group */
        if (-1 == setsid())
        {
            perror("setsid");
            return -1;
        }

        /* set the working directory to the root directory */
        if (-1 == chdir("/"))
        {
            perror("setsid");
            return -1;
        }

        /* close all open files */
        close(0);
        close(1);
        close(2);

        /* redirect fd's 0,1,2 to /dev/null */
        open("/dev/null", O_RDWR); /* stdin */
        dup(0);                    /* stdout */
        dup(0);
    }

    // make socket non-blocking
    if (-1 == fcntl(sockfd, F_SETFL, O_NONBLOCK))
    {
        perror("fcntl");
        return -1;
    }

    // listen()
    if (-1 == listen(/*sockfd*/ sockfd, /*backlog*/ BACKLOG))
    {
        perror("listen");
        return -1;
    }
    printf("server: socket listening\n");

    // open /var/tmp/aesdsocketdata for write
    outputfd = open(OUTPUT_FILE_PATH, O_CREAT | O_APPEND | O_RDWR | O_SYNC, 0644);
    if (-1 == outputfd)
    {
        perror("open");
        return -1;
    }
    printf("server: output file opened\n");

    // allocate heap
    buf = malloc(MAXBUFLEN);

    while (!signaled)
    {
        // accept
        addr_len = sizeof their_addr;

        printf("server: accepting...\n");
        do
        {
            acceptfd = accept(/*sockfd*/ sockfd, /*addr*/ (struct sockaddr *)&their_addr, /*addrlen*/ &addr_len);
        } while (-1 == acceptfd && EAGAIN == errno && !signaled);
        if (-1 == acceptfd)
        {
            if (signaled)
            {
                break;
            }
            else
            {
                perror("accept");
                return -1;
            }
        }
        printf("server: accepted\n");

        // syslog accepted connection
        syslog(LOG_INFO, "Accepted connection from %s",
               inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));

        // recv loop while client connected
        do
        {
            numbytes, recvbytes = 0;
            precvbuf = buf;

            do
            {
                do
                {
                    numbytes = recv(/*sockfd*/ acceptfd, /*buf*/ precvbuf, /*len*/ MAXBUFLEN - recvbytes - 1, /*flags*/ 0);
                } while (-1 == numbytes && EAGAIN == errno);
                if (-1 == numbytes)
                {
                    perror("recv");
                    return -1;
                }
                else if (numbytes > 0)
                {
                    precvbuf += numbytes;
                    recvbytes += numbytes;
                }
                else
                {
                    // connection terminated
                    break;
                }

            } while (buf[recvbytes - 1] != '\n');
            buf[recvbytes] = '\0';
            printf("server: received ---\n%s\n--- bytes=%d\n", buf, recvbytes);

            // handle connection closed
            if (0 == recvbytes)
            {
                printf("server: connection closed");
                break;
            }

            // write to /var/tmp/aesdsocketdata
            if (-1 == write(/*fd*/ outputfd, /*buf*/ buf, /*count*/ recvbytes))
            {
                perror("write");
                return -1;
            }
            printf("server: wrote ---\n%s\n--- bytes=%d\n", buf, recvbytes);

            // seek to start of file
            if (-1 == lseek(/*fd*/ outputfd, /*pos*/ 0, /*origin*/ SEEK_SET))
            {
                perror("lseek");
                return -1;
            }
            printf("server: lseek done\n");

            // read everything from /var/tmp/aesdsocketdata
            pbuf = buf;
            while (0 != (ret = read(/*fd*/ outputfd, /*buf*/ pbuf, /*len*/ MAXBUFLEN - (pbuf - buf) - 1)))
            {
                if (-1 == ret)
                {
                    if (errno == EINTR)
                        continue;
                    perror("read");
                    break;
                }
                pbuf += ret;
            }
            *pbuf = '\0';
            printf("server: read ---\n%s\n--- bytes=%ld\n", buf, pbuf - buf);

            // send back to client
            if (-1 == (numbytes = send(/*sockfd*/ acceptfd, /*msg*/ buf, /*len*/ pbuf - buf, /*flags*/ 0)))
            {
                perror("send");
                return -1;
            }
            printf("server: sent ---\n%s\n--- bytes=%ld\n", buf, pbuf - buf);
        } while (0 != recvbytes && !signaled);

        // if (-1 == shutdown(acceptfd, SHUT_RDWR))
        // {
        //     perror("shutdown-acceptfd");
        //     return -1;
        // }
        if (-1 == close(acceptfd))
        {
            perror("close-acceptfd");
            return -1;
        }
        printf("server: closed acceptfd\n");

        // syslog closed connection
        syslog(LOG_INFO, "Closed connection from %s",
               inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));
    }

    free(buf);
    printf("server: freed buf\n");

    // close everything
    if (-1 == close(outputfd))
    {
        perror("close-outputfd");
        return -1;
    }
    printf("server: closed outputfd\n");

    if (-1 == remove(OUTPUT_FILE_PATH))
    {
        perror("remove");
        return -1;
    }
    printf("server: removed /var/tmp/aesdsocketdata\n");

    if (-1 == shutdown(sockfd, SHUT_RDWR))
    {
        perror("shutdown-sockfd");
        return -1;
    }
    if (-1 == close(sockfd))
    {
        perror("close-sockfd");
        return -1;
    }
    printf("server: closed sockfd\n");

    closelog();

    return 0;
}
