#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <signal.h>
#include "libevent_http.h"

void listen_cb(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* addr, int len, void *ptr)
{
	event_base* base = (struct event_base*)ptr;

	Http* http = Http::create(base, fd);
	http->run(http);
}

void signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	struct event_base* base = (struct event_base*)user_data;
	struct timeval delay = { 1, 0 };
	printf("catch a signal!\n");
	event_base_loopexit(base, &delay);
}

int main(int argc, char* argv[])
{
	if (argc<3)
	{
		printf("./a.out port filename\n");
		exit(1);
	}
	chdir(argv[2]);

	struct event_base* base;
	struct evconnlistener* listener;
	struct event* signal_event;

	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(argv[1]));
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	base = event_base_new();

	listener = evconnlistener_new_bind(base, listen_cb, (void*)base,
		LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (struct sockaddr*)&server, sizeof(server));
	if (!listener)
	{
		fprintf(stderr, "can not create a listenner\n");
		return 1;
	}

	signal_event = evsignal_new(base, SIGINT, signal_cb, (void*)base);
	if (!signal_event || (event_add(signal_event, NULL))<0)
	{
		fprintf(stderr, "can not create signal_event/add signal_event\n");
		return 1;
	}
	event_base_dispatch(base);

	evconnlistener_free(listener);
	event_free(signal_event);
	event_base_free(base);
	return 0;
}
