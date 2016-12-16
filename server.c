/* echo server */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include "epoll_server.h"

#define SERVER_PORT 7788

#define BUFSIZE 64   //fixed echo packet lenth 64

int create_listen_fd(const char* address);
void event_clean_handler(epoll_event_t *ev);
void event_read_handler(epoll_event_t *ev);
void event_write_handler(epoll_event_t *ev);
void event_listen_handler(epoll_event_t *ev);
void event_empty_handler(epoll_event_t *ev);
void event_error_handler(epoll_event_t *ev);
int add_listen_fd(int fd);

typedef struct {
    u_char   buf[BUFSIZE];
    int      r;
    int      w;
} echo_t;

int create_listen_fd(const char* address)
{
    int                fd;
    socklen_t          socklen;
    struct sockaddr_in server;

    fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);

    inet_pton(AF_INET, address, &server.sin_addr);

    socklen = sizeof(struct sockaddr_in);

    if (bind(fd, (struct sockaddr*)&server, socklen) < 0) {
        return -1;
    }

    listen(fd, 511);

    return fd;
}

void event_clean_handler(epoll_event_t *ev)
{
    if (ev->data) {
        free(ev->data);
    }
}

void event_read_handler(epoll_event_t *ev)
{
    ssize_t   n;
    echo_t   *echo;

    echo = (echo_t *)ev->data;

    do {
        n = recv(ev->fd, echo->buf + echo->r, BUFSIZE - echo->r, 0);

        if (n == 0) {
            epoll_free_event(ev);
            //ev->clean_handler(ev);
            return;
        }

        if (n > 0) {
            
            echo->r += n;

            if (echo->r == BUFSIZE) {

                if (epoll_del_event(ev, EPOLL_RD_EVENT) != EPOLL_OK) {
                    return ev->error_handler(ev);
                }

                ev->write_handler = event_write_handler;

                return ev->write_handler(ev);
            }
        }

        if (errno == EAGAIN) {

            if (epoll_add_event(ev, EPOLL_RD_EVENT, EPOLL_ET) != EPOLL_OK) {
                return ev->error_handler(ev);
            }

            return ;
        }

        if (errno != EINTR) {
            return ev->error_handler(ev);
        }

    } while(1);

    return;
}

void event_write_handler(epoll_event_t *ev)
{
    ssize_t   n;
    echo_t   *echo;

    echo = (echo_t *)ev->data;

    do {
        n = send(ev->fd, echo->buf + echo->w, echo->r - echo->w, 0);

        if (n > 0) {
            echo->w += n;

            if (echo->w == echo->r) {
                epoll_free_event(ev);
                return;
            }
        }

        if (n == 0) {
            fprintf(stderr, "send return zero\n");
        }

        if (errno == EAGAIN ) {

            if (epoll_add_event(ev, EPOLL_WR_EVENT, EPOLL_ET) != EPOLL_OK) {
                return ev->error_handler(ev);
            }

            return ;           
        }

        if (errno != EINTR) {
            return ev->error_handler(ev);
        }

    } while(1);

}

void event_listen_handler(epoll_event_t *ev)
{
    int                 connfd;
    socklen_t           psize;
    struct sockaddr_in  peer;
    epoll_event_t      *e;
    echo_t             *echo;

    psize = sizeof(struct sockaddr_in);

    connfd = accept4(ev->fd, (struct sockaddr *)&peer, &psize, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd < 0) {
        fprintf(stderr, "accept4 failed: %s\n", strerror(errno));
        return;
    }

    e = epoll_get_event(connfd);
    if (e == NULL) {
        fprintf(stderr, "epoll_get_event failed\n");
        return;
    }

    e->read_handler = event_read_handler;
    e->write_handler = event_empty_handler;
    e->error_handler = event_error_handler;
    e->clean_handler = event_clean_handler;

    e->data = malloc(sizeof(echo_t));
    if (!e->data) {
        ev->error_handler(ev);
        return;
    }

    echo = e->data;
    
    echo->r = 0;
    echo->w = 0;

    return e->read_handler(e);
}

void event_empty_handler(epoll_event_t *ev)
{
    fprintf(stderr, "unexpectedly handler happened\n");
}

void event_error_handler(epoll_event_t *ev)
{
    fprintf(stderr, "event with fd: %d errors\n", ev->fd);
    epoll_free_event(ev);
}

int add_listen_fd(int fd)
{
    epoll_event_t *ev;

    ev = epoll_get_event(fd);
    if (ev == NULL) {
        return -1;
    }

    ev->read_handler = event_listen_handler;
    ev->write_handler = event_empty_handler;
    ev->error_handler = event_error_handler;

    if (epoll_add_event(ev, EPOLL_RD_EVENT, 0)) {
        return -1;
    }

    return 0;
}

int main(int argc, const char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "parameter is less than 2\n");
        exit(-1);
    }

    int listenfd;

    listenfd = create_listen_fd(argv[1]);
    if (listenfd < 0) {
        fprintf(stderr, "create_listen_fd failed\n");
        exit(-1);
    }

    if (epoll_init(0)) {
        fprintf(stderr, "epoll_init failed\n");
        exit(-1);
    }

    if(add_listen_fd(listenfd)) {
        fprintf(stderr, "add_listen_fd failed\n");
        exit(-1);
    }

    int ret;

    for (;;) {
        ret = epoll_process_event(-1);

        if (ret) {
            fprintf(stderr, "epoll_process_event failed: %s\n", strerror(ret));
        }
    }
}
