#include <iostream>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event.h>


class Http
{
private:
	static void read_cb(bufferevent* bev, void* arg);
	static void event_cb(bufferevent* bev, short events, void* arg);
private:
	/*explict*/ Http(event_base*, evutil_socket_t);
	~Http();

private:
	bufferevent* bev;

private:
	void response_http(bufferevent* bev, char* path);
	void send_error(bufferevent* bev);
	void send_head(bufferevent* bev, int num, const char *status, const char* type, long size);
    void send_file(bufferevent* bev, const char* filename);
	void send_dir(bufferevent* bev, const char* dirname);
    void strdecode(char *to, char *from);
	int hexit(char c);
	void strencode(char* to, size_t tosize, const char* from);
    const char *get_file_type(const char *name);
	
public:
	static Http* create(event_base* base, evutil_socket_t fd);
	static void release(Http**);

	void run(void* arg);
};
