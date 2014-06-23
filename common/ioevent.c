#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "ioevent.h"

#if IOEVENT_USE_KQUEUE
/* we define these here as numbers, because for kqueue mapping them to a combination of
     * filters / flags is hard to do. */
int kqueue_ev_convert(int16_t event, uint16_t flags)
{
  int r;

  if (event == EVFILT_READ) {
    r = KPOLLIN;
  }
  else if (event == EVFILT_WRITE) {
    r = KPOLLOUT;
  }
  else {
    r = 0;
  }

  if (flags & EV_EOF) {
    r |= KPOLLHUP;
  }
  return r;
}
#endif

int ioevent_init(IOEventPoller *ioevent, const int size,
    const int timeout, const int extra_events)
{
  int bytes;

  ioevent->size = size;
  ioevent->extra_events = extra_events;

#if IOEVENT_USE_EPOLL
  ioevent->timeout = timeout;
  ioevent->poll_fd = epoll_create(ioevent->size);
  bytes = sizeof(struct epoll_event) * size;
  ioevent->events = (struct epoll_event *)malloc(bytes);
#elif IOEVENT_USE_KQUEUE
  ioevent->timeout.tv_sec = timeout / 1000;
  ioevent->timeout.tv_nsec = 1000000 * (timeout % 1000);
  ioevent->poll_fd = kqueue();
  bytes = sizeof(struct kevent) * size;
  ioevent->events = (struct kevent *)malloc(bytes);
#elif IOEVENT_USE_PORT
  ioevent->timeout.tv_sec = timeout / 1000;
  ioevent->timeout.tv_nsec = 1000000 * (timeout % 1000);
  ioevent->poll_fd = port_create();
  bytes = sizeof(port_event_t) * size;
  ioevent->events = (port_event_t *)malloc(bytes);
#endif
  if (ioevent->events == NULL) {
    return errno != 0 ? errno : ENOMEM;
  }
  return 0;
}

void ioevent_destroy(IOEventPoller *ioevent)
{
  if (ioevent->events != NULL) {
    free(ioevent->events);
    ioevent->events = NULL;
  }

  if (ioevent->poll_fd >=0) {
    close(ioevent->poll_fd);
    ioevent->poll_fd = -1;
  }
}

int ioevent_attach(IOEventPoller *ioevent, const int fd, const int e,
    void *data)
{
#if IOEVENT_USE_EPOLL
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.events = e | ioevent->extra_events;
  ev.data.ptr = data;
  return epoll_ctl(ioevent->poll_fd, EPOLL_CTL_ADD, fd, &ev);
#elif IOEVENT_USE_KQUEUE
  struct kevent ev[2];
  int n = 0;
  if (e & IOEVENT_READ) {
    EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD | ioevent->extra_events, 0, 0, data);
  }
  if (e & IOEVENT_WRITE) {
    EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD | ioevent->extra_events, 0, 0, data);
  }
  return kevent(ioevent->poll_fd, ev, n, NULL, 0, NULL);
#elif IOEVENT_USE_PORT
  return port_associate(ioevent->poll_fd, PORT_SOURCE_FD, fd, e, data);
#endif
}

int ioevent_modify(IOEventPoller *ioevent, const int fd, const int e,
    void *data)
{
#if IOEVENT_USE_EPOLL
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.events = e | ioevent->extra_events;
  ev.data.ptr = data;
  return epoll_ctl(ioevent->poll_fd, EPOLL_CTL_MOD, fd, &ev);
#elif IOEVENT_USE_KQUEUE
  struct kevent ev[2];
  int n = 0;
  if (e & IOEVENT_READ) {
    EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD | ioevent->extra_events, 0, 0, data);
  }
  else {
    EV_SET(&ev[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, data);
  }

  if (e & IOEVENT_WRITE) {
    EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD | ioevent->extra_events, 0, 0, data);
  }
  else {
    EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, data);
  }
  return kevent(ioevent->poll_fd, ev, n, NULL, 0, NULL);
#elif IOEVENT_USE_PORT
  return port_associate(ioevent->poll_fd, PORT_SOURCE_FD, fd, e, data);
#endif
}

int ioevent_detach(IOEventPoller *ioevent, const int fd)
{
#if IOEVENT_USE_EPOLL
  return epoll_ctl(ioevent->poll_fd, EPOLL_CTL_DEL, fd, NULL);
#elif IOEVENT_USE_PORT
  return port_dissociate(ioevent->poll_fd, PORT_SOURCE_FD, fd);
#else
  return 0;
#endif
}

int ioevent_poll(IOEventPoller *ioevent)
{
#if IOEVENT_USE_EPOLL
  return epoll_wait(ioevent->poll_fd, ioevent->events, ioevent->size, ioevent->timeout);
#elif IOEVENT_USE_KQUEUE
  return kevent(ioevent->poll_fd, NULL, 0, ioevent->events, ioevent->size, &ioevent->timeout);
#elif IOEVENT_USE_PORT
  int result;
  int retval;
  unsigned int nget = 1;
  if((retval = port_getn(ioevent->poll_fd, ioevent->events,
          ioevent->size, &nget, &ioevent->timeout)) == 0)
  {
    result = (int)nget;
  } else {
    switch(errno) {
      case EINTR:
      case EAGAIN:
      case ETIME:
        if (nget > 0) {
          result = (int)nget;
        }
        else {
          result = 0;
        }
        break;
      default:
        result = -1;
        break;
    }
  }
  return result;
#else
#error port me
#endif
}

