#ifndef __epoll_server_h__
#define __epoll_server_h__

#include <sys/epoll.h>

#define EPOLL_RD_EVENT EPOLLIN | EPOLLRDHUP
#define EPOLL_WR_EVENT EPOLLOUT 

#define EPOLL_ET EPOLLET

#define EPOLL_OK   0
#define EPOLL_ERR -1

typedef struct epoll_event_s epoll_event_t;

typedef void (*epoll_event_handler)(epoll_event_t *ev);

struct epoll_event_s {
	int                   fd;
	int                   event;
	int                   read;
	int                   write;
	void                 *data;
	epoll_event_handler   read_handler;
    epoll_event_handler   write_handler;
    epoll_event_handler   error_handler;
    epoll_event_handler   clean_handler;
};


int epoll_init(int size);
void epoll_close(void);

int epoll_add_event(epoll_event_t *ev, int event, int flags);
int epoll_del_event(epoll_event_t *ev, int event);
int epoll_process_event(uint64_t timer);

epoll_event_t *epoll_get_event(int fd);
void epoll_free_event(epoll_event_t *ev);



#endif
