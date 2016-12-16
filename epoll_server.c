#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>


#include "epoll_server.h"

#define EPOLL_SIZE        32

#ifndef EPOLL_EVENT_PAIRS
#define EPOLL_EVENT_PAIRS (65536 << 1)
#endif

static int epollfd;


static epoll_event_t *event_list;


int epoll_init(int size)
{
	int i, esize;

	esize = size > EPOLL_EVENT_PAIRS ? size : EPOLL_EVENT_PAIRS;

    event_list = (epoll_event_t *)malloc(sizeof(epoll_event_t) * esize);
    if (event_list == NULL) {
        return EPOLL_ERR;
    }

    for (i = 0; i < esize; i++) {
        event_list[i].fd = -1;
    }

	epollfd = epoll_create(EPOLL_SIZE);

	return epollfd >= 0 ? EPOLL_OK : EPOLL_ERR;
}

epoll_event_t *epoll_get_event(int fd)
{
    if (fd >= EPOLL_EVENT_PAIRS || event_list[fd].fd != -1) {
        return NULL;
    }

    memset(&event_list[fd], 0x00, sizeof(epoll_event_t));

    event_list[fd].fd = fd;

    return &event_list[fd];
}

void epoll_free_event(epoll_event_t *ev)
{
    epoll_del_event(ev, EPOLL_RD_EVENT);
    epoll_del_event(ev, EPOLL_WR_EVENT);

    if (ev->clean_handler) {
        ev->clean_handler(ev);
    }

    close(ev->fd);

    event_list[ev->fd].fd = -1;

    return;
}

int epoll_add_event(epoll_event_t *ev, int event, int flags)
{
	int                  op;
	uint32_t             events, prev;
	struct epoll_event   ee;

	if (event == EPOLL_RD_EVENT) {
        if (ev->read) {
            return EPOLL_OK;
        }

		prev = EPOLL_WR_EVENT;
		events = EPOLL_RD_EVENT;
	} else {
        if (ev->write) {
            return EPOLL_OK;
        }
		prev = EPOLL_RD_EVENT;
		events = EPOLL_WR_EVENT;
	}

	if (ev->read || ev->write) {
		op = EPOLL_CTL_MOD;
		events |= prev;
	} else {
		op = EPOLL_CTL_ADD;
	}

	ee.events = events | flags;
    ee.data.ptr = (void *)ev;

    if (epoll_ctl(epollfd, op, ev->fd, &ee) == -1) {
        return EPOLL_ERR;
    }

    event == EPOLL_RD_EVENT ? (ev->read = ee.events) : (ev->write = ee.events);

    return EPOLL_OK;
}

int epoll_del_event(epoll_event_t *ev, int event)
{
    int                  op, e;
    uint32_t             pre;
    struct epoll_event   ee;

    if (event == EPOLL_RD_EVENT) {
        if (!ev->read) 
            return EPOLL_OK;
        
        e = ev->write;
    } else {
        if (!ev->write) 
            return EPOLL_OK;
        
        e = ev->read;
    }

    if (e) {
        op = EPOLL_CTL_MOD;
        ee.events = e;
        ee.data.ptr = (void *) ev;
    } else {
        op = EPOLL_CTL_DEL;
        ee.events = 0;
        ee.data.ptr = NULL; 
    }

    if (epoll_ctl(epollfd, op, ev->fd, &ee) == -1) {
        return EPOLL_ERR;
    }

    event == EPOLL_RD_EVENT ? (ev->read = 0) : (ev->write = 0);

    return EPOLL_OK;
}

int epoll_process_event(uint64_t timer)
{
    int                          events, err, i;
    uint32_t                     revent;
    epoll_event_t               *e;
    static struct epoll_event    revent_list[1024]; 

    events = epoll_wait(epollfd, revent_list, 1024, timer);

    err = errno;

    if (events == -1) {
        return err; 
    }

    if (events == 0) {
        return EPOLL_OK;
    }

    for (i = 0; i < events; i++) {
        e = (epoll_event_t *)revent_list[i].data.ptr;

        if (e->fd == -1) {
            continue;
        }

        revent = revent_list[i].events;

        if (revent & (EPOLLERR|EPOLLHUP)) {
            e->error_handler(e);
        }

        if ((revent & EPOLL_RD_EVENT) && e->read) {
            e->read_handler(e);
        }

        if ((revent & EPOLL_WR_EVENT) && e->write) {
            e->write_handler(e);
        }
    }

    return EPOLL_OK;
}


void epoll_close(void)
{
	close(epollfd);
}



