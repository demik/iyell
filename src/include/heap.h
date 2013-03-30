#ifndef HEAP_H
#define HEAP_H

/* structures for events */
extern struct event    ev_irc;
extern struct event    ev_udp4;
extern struct event    ev_udp6;
extern struct event    ev_unix;

/* IO buffers */
extern struct evbuffer	*ircin, *ircout; 
extern struct evbuffer	*udpin, *unixfifo;

/* timeouts */
extern struct timeval	timeout;

#endif
