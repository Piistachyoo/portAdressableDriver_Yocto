#include "aesd_ioctl.h"
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define LOG_PRIO(_LEVEL) (LOG_USER | _LEVEL)
#define BUFF_MAX_LEN 5000000

#define USE_AESD_CHAR_DEVICE 1

#ifndef USE_AESD_CHAR_DEVICE
#define PATH "/var/tmp/aesdsocketdata"
#else
#define PATH "/dev/aesdchar"
#define IOCTL_CMD "AESDCHAR_IOCSEEKTO:"
#define WRITE_CMD 19
#define WRITE_CMD_OFF 21
#endif

struct node {
    pthread_t thread;
    struct node *next;
} thread_list;
#ifndef USE_AESD_CHAR_DEVICE
pthread_t timer_thread_handle;
#endif
int mySocketFD, peerSocketFD;
FILE *file;
pthread_mutex_t file_mutex;
struct sockaddr peeradd;
struct node *HEAD = NULL;
uint8_t ioctl_sent = 0;

uint8_t check_ioctl(char *string) {
    uint8_t index;
    char *cmd = "AESDCHAR_IOCSEEKTO:";
    for (index = 0; index < strlen(cmd); index++) {
        if (string[index] != cmd[index]) {
            printf("They don't match\n");
            return 1;
        }
    }
    printf("They match\n");
    return 0;
}

void signal_handler(int signal) {
    printf("Caught signal SIGINT!\n");
    printf("Terminating...\n");
//	struct node *iterator = HEAD;
//	struct node *temp = iterator;
#ifndef USE_AESD_CHAR_DEVICE
    pthread_cancel(timer_thread_handle);
    pthread_join(timer_thread_handle, NULL);
#endif
    //	while(iterator){
    // pthread_cancel(iterator->thread);
    // iterator = iterator->next;
    /*
    free(iterator);
    iterator = temp->next;
    temp = temp->next;
    */
    //	}
    close(mySocketFD);
    close(peerSocketFD);
#ifndef USE_AESD_CHAR_DEVICE
    remove(PATH);
#endif
    shutdown(mySocketFD, SHUT_RDWR);
    exit(0);
}

int initiate_connection(void) {
    int yes = 1;
    struct addrinfo ad_info;
    struct addrinfo *servinfo;
    mySocketFD = socket(PF_INET, SOCK_STREAM, 0);
    memset(&ad_info, 0, sizeof(ad_info));
    ad_info.ai_flags = AI_PASSIVE;
    ad_info.ai_family = AF_INET;
    ad_info.ai_socktype = SOCK_STREAM;
    ad_info.ai_protocol = 0;
    if (getaddrinfo(NULL, "9000", &ad_info, &servinfo) != 0) {
        perror("getaddrinfo");
        return -1;
    } else { /* Do Nothing */
    }

    if (setsockopt(mySocketFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) ==
        -1) {
        perror("setsockopt");
        return -1;
    } else { /* Do Nothing */
    }

    if (bind(mySocketFD, servinfo->ai_addr, sizeof(struct addrinfo)) != 0) {
        perror("bind");
        freeaddrinfo(servinfo);
        return -1;
    } else { /* Do Nothing */
    }

    freeaddrinfo(servinfo);

    if (listen(mySocketFD, 1) != 0) {
        perror("listen");
        return -1;
    } else { /* Do Nothing */
    }

    return 0;
}

void receive_data(void) {
    syslog(LOG_PRIO(LOG_DEBUG), "Entered function\n");
    char my_buffer[BUFF_MAX_LEN];

    int index = 0;
    int buffer_len = 0;
    syslog(LOG_PRIO(LOG_DEBUG), "Started receiving data\n");
    while (1) {
        recv(peerSocketFD, &(my_buffer[index]), 1, MSG_WAITALL);
        buffer_len++;
        if (my_buffer[index] == '\n') {
            break;
        }
        index++;
    }
    syslog(LOG_PRIO(LOG_DEBUG), "**********************************\n");
    syslog(LOG_PRIO(LOG_DEBUG), "The sent string is: %s\n", my_buffer);
    syslog(LOG_PRIO(LOG_DEBUG), "**********************************\n");
    printf("The sent string is: %s\n", my_buffer);
    // if (strncmp(IOCTL_CMD, my_buffer, strlen("AESDCHAR_IOC")) == 0) {
    //     printf("Entered ioctl\n");
    //     ioctl_sent = 1;
    //     struct aesd_seekto seekto;
    //     seekto.write_cmd = my_buffer[WRITE_CMD] - '0';
    //     seekto.write_cmd_offset = my_buffer[WRITE_CMD_OFF] - '0';
    //     syslog(LOG_PRIO(LOG_DEBUG), "Calling ioctl with cmd = %lu\n",
    //            AESDCHAR_IOCSEEKTO);
    //     printf("Calling IOCTL with cmd = %lu\n", AESDCHAR_IOCSEEKTO);
    //     printf("(%u, %u)\n", seekto.write_cmd, seekto.write_cmd_offset);
    //     printf("(%c, %c)\n", my_buffer[WRITE_CMD], my_buffer[WRITE_CMD_OFF]);
    //     file = fopen(PATH, "a");
    //     if (ioctl(fileno(file), AESDCHAR_IOCSEEKTO, &seekto)) {
    //         syslog(LOG_PRIO(LOG_DEBUG), "Error calling ioctl\n");
    //     }
    //     fclose(file);
    // }
    if (check_ioctl(my_buffer) == 0) {
        ioctl_sent = 1;
        struct aesd_seekto seekto;
        seekto.write_cmd = my_buffer[WRITE_CMD] - '0';
        seekto.write_cmd_offset = my_buffer[WRITE_CMD_OFF] - '0';
        syslog(LOG_PRIO(LOG_DEBUG), "Calling ioctl with cmd = %u\n",
               AESDCHAR_IOCSEEKTO);
        printf("Calling IOCTL with cmd = %u\n", AESDCHAR_IOCSEEKTO);
        printf("(%u, %u)\n", seekto.write_cmd, seekto.write_cmd_offset);
        printf("(%c, %c)\n", my_buffer[WRITE_CMD], my_buffer[WRITE_CMD_OFF]);
        file = fopen(PATH, "a");
        if (ioctl(fileno(file), AESDCHAR_IOCSEEKTO, &seekto)) {
            syslog(LOG_PRIO(LOG_DEBUG), "Error calling ioctl\n");
        }
        fclose(file);
    } else {
        syslog(LOG_PRIO(LOG_DEBUG), "Opening file\n");
        file = fopen(PATH, "a");
        syslog(LOG_PRIO(LOG_DEBUG), "Writing to file\n");
        fwrite(my_buffer, sizeof(char), buffer_len, file);
        syslog(LOG_PRIO(LOG_DEBUG), "Closing file\n");
        fclose(file);
    }
}

void send_data(void) {
    char *str_buffer = (char *)malloc(BUFF_MAX_LEN);
    file = fopen(PATH, "r");
    while (fgets(str_buffer, BUFF_MAX_LEN, file)) {
        send(peerSocketFD, str_buffer, strlen(str_buffer), MSG_WAITALL);
    }
    fclose(file);

    printf("Closed connection from %u:%u:%u:%u\n", peeradd.sa_data[2],
           peeradd.sa_data[3], peeradd.sa_data[4], peeradd.sa_data[5]);
    syslog(LOG_PRIO(LOG_DEBUG), "Closed connection from %u:%u:%u:%u\n",
           peeradd.sa_data[2], peeradd.sa_data[3], peeradd.sa_data[4],
           peeradd.sa_data[5]);
    free(str_buffer);
}

void *start_thread(void *arg) {
    // syslog(LOG_PRIO(LOG_DEBUG), "Acquiring mutex\n");
    pthread_mutex_lock(&file_mutex);
    syslog(LOG_PRIO(LOG_DEBUG), "Acquired mutex\n");
    syslog(LOG_PRIO(LOG_DEBUG), "Calling receive_data()\n");
    receive_data();
    syslog(LOG_PRIO(LOG_DEBUG), "End of function receive_data()\n");
    send_data();
    syslog(LOG_PRIO(LOG_DEBUG), "Sent data\n");
    pthread_mutex_unlock(&file_mutex);
    syslog(LOG_PRIO(LOG_DEBUG), "Unlocked mutex\n");
    pthread_exit(arg);
}
#ifndef USE_AESD_CHAR_DEVICE
void *timer_thread(void *arg) {
    int old;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
    char timestr[200];
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    while (1) {
        sleep(10);
        tmp = localtime(&t);
        if (tmp == NULL) {
            perror("localtime");
            exit(EXIT_FAILURE);
        }
        if (strftime(timestr, sizeof(timestr), "%a, %d %b %Y %T %z", tmp) ==
            0) {
            fprintf(stderr, "strftime returned 0");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_lock(&file_mutex);
        file = fopen(PATH, "a");
        fwrite("timestamp:", sizeof(char), strlen("timestamp:"), file);
        fwrite(timestr, sizeof(char), strlen(timestr), file);
        fwrite("\n", sizeof(char), strlen("\n"), file);
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
    }
}
#endif
int main(int argc, char *argv[]) {
    int ret;
    if (argc > 1) {
        if (strcmp(argv[1], "-d") == 0) {
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                return -1;
            } else if (pid != 0) {
                exit(0);
            } else { /* Do Nothing */
            }
            if (setsid() == -1) {
                perror("setsid");
                return -1;
            } else { /* Do Nothing */
            }
            file = freopen("/dev/null", "r", stdin);
            file = freopen("/dev/null", "w", stdout);
            file = freopen("/dev/null", "w", stderr);
            file = NULL;
        }
    }
    /* Set signal handler */
    signal(SIGINT, signal_handler);

    if (initiate_connection() != 0) {
        return -1;
    }

/* Truncate file */
#ifndef USE_AESD_CHAR_DEVICE
    file = fopen(PATH, "w");
    fclose(file);
#endif
#ifndef USE_AESD_CHAR_DEVICE
    pthread_create(&timer_thread_handle, NULL, timer_thread, (void *)NULL);
#endif
    while (1) {
        socklen_t len = sizeof(struct sockaddr);
        peerSocketFD = accept(mySocketFD, &peeradd, &len);
        if (peerSocketFD == -1) {
            perror("accept");
            continue;
        }
        printf("Accepted connection from %u:%u:%u:%u\n", peeradd.sa_data[2],
               peeradd.sa_data[3], peeradd.sa_data[4], peeradd.sa_data[5]);
        syslog(LOG_PRIO(LOG_DEBUG), "Accepted connection from %u:%u:%u:%u\n",
               peeradd.sa_data[2], peeradd.sa_data[3], peeradd.sa_data[4],
               peeradd.sa_data[5]);
        struct node *new = (struct node *)malloc(sizeof(struct node));
        if (new == NULL) {
            syslog(LOG_PRIO(LOG_DEBUG), "Error allocating memory\n");
            continue;
        }
        syslog(LOG_PRIO(LOG_DEBUG), "Creating thread\n");
        ret = pthread_create(&(new->thread), NULL, start_thread, (void *)NULL);
        if (0 != ret) {
            syslog(LOG_PRIO(LOG_ERR), "Error creating thread!");
        }
        if (HEAD == NULL) {
            HEAD = new;
            new->next = NULL;
        } else {
            new->next = NULL;
            HEAD = new;
        }
        syslog(LOG_PRIO(LOG_DEBUG), "Thread creation done!\n");
        pthread_join(new->thread, NULL);
        free(new);
    }
    return 0;
}