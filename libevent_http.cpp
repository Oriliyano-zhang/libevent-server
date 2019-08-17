#include <stdio.h>                                                                                                           
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <iostream>
#include <string>
#include "libevent_http.h"

void Http::response_http(bufferevent* bev, char* path)
{
    strdecode(path, path);
    char *file = &path[1];
    if (strcmp(path, "/") == 0)//not file
    {
        char files[2];
        std::string s="./";
        strcpy(files,s.c_str());
        file=files;
    }
    printf("path:%s  file:%s\n", path, file);
    struct stat sb;
    int ret = stat(file, &sb);
    if (ret == -1)
    {
        perror("open files error");
        send_error(bev);
        return;
    }

    if (S_ISDIR(sb.st_mode))
    {
        send_head(bev, 200, "ok", get_file_type(".html"), -1);
        send_dir(bev, file);
    }
    else if (S_ISREG(sb.st_mode))
    {
        send_head(bev, 200, "ok", get_file_type(file), sb.st_size);// not -1
        send_file(bev, file);
    }
}

void Http::send_error(bufferevent* bev)
{
    send_head(bev, 404, "File Note Found!", ".html", -1);
    send_file(bev, "404.html");
}

void Http::send_head(bufferevent* bev, int num, const char *status, const char* type, long size)
{
    char buf[1024] = { 0 };
    //求情行
    sprintf(buf, "HTTP/1.1 %d %s\r\n", num, status);
    bufferevent_write(bev, buf, strlen(buf));
    //请求头
    sprintf(buf, "Content-type:%s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length:%ld\r\n", size);
    bufferevent_write(bev, buf, strlen(buf));
    //空行
    bufferevent_write(bev, "\r\n", 2);
}
//发送文件
void Http::send_file(bufferevent* bev, const char* filename)
{
    int fd = open(filename, O_RDONLY);//"filename"
    if (fd == -1)
    {
        perror("open file error");
        return;
    }
    char buf[4096] = { 0 };
    int len = 0;
    while ((len = read(fd, buf, sizeof(buf))))
    {
        bufferevent_write(bev, buf, len);
        memset(buf, 0, strlen(buf));
    }
    close(fd);
}

void Http::send_dir(bufferevent* bev, const char* dirname)
{
    char buf[4096] = { 0 };
    sprintf(buf, "<html><head><meta charset=\"utf-8\"><title>Directory:%s</title></head>", dirname);
    sprintf(buf + strlen(buf), "<body><h1>Current Directory:%s</h1><table>", dirname);

    char path[1024];
    char encode_name[1024];
    char timestr[64];
    struct dirent **dirinfo;
    int num = scandir(dirname, &dirinfo, NULL, alphasort);
    for (int i = 0; i<num; i++)
    {
        char* name = dirinfo[i]->d_name;
        strencode(encode_name, sizeof(encode_name), name);
        sprintf(path, "%s%s", dirname, name);
        printf("path--------%s\n", path);
        struct stat sb;
        stat(path, &sb);

        strftime(timestr, sizeof(timestr), "%d %b %Y %H:%M", localtime(&sb.st_mtime));
        if (S_ISREG(sb.st_mode))
        {
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td><td>%s</td></tr>"
                    , encode_name, name, (long)sb.st_size, timestr);//   </a>
        }
        else if (S_ISDIR(sb.st_mode))
        {
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td><td>%s</td></tr>"
                    , encode_name, name, (long)sb.st_size, timestr);
        }
        bufferevent_write(bev, buf, strlen(buf));
        memset(buf, 0, sizeof(buf));
    }
    sprintf(buf + strlen(buf), "</table></body></html>");//strlen(buf) 
    bufferevent_write(bev, buf, strlen(buf));
    printf("dir send ok\n");
}

void Http::read_cb(bufferevent* bev, void* arg)
{
    Http* http=(Http*)arg;
    char buf[4096] = { 0 };
    char method[12], path[1024], protocol[12];
    bufferevent_read(bev, buf, sizeof(buf));
    printf("buf:%s\n", buf);
    sscanf(buf, "%[^ ] %[^ ] %[^ \r\n]", method, path, protocol);
    printf("method:%s  path:%s  protocol:%s\n", method, path, protocol);
    if (strcasecmp(method, "GET") == 0)
    {
        http-> response_http(bev, path);
    }
}

void Http::event_cb(bufferevent* bev, short events, void* arg)
{
    if (events&BEV_EVENT_EOF)
    {
        printf("conncted closed!\n");
    }
    else if (events&BEV_EVENT_ERROR)
    {
        printf("some other wrong!\n");
    }
    bufferevent_free(bev);
}

void Http::run(void* arg)
{
    bufferevent_setcb(bev, read_cb, NULL, event_cb, arg);
    bufferevent_enable(bev, EV_READ);
}

Http::Http(event_base* base, evutil_socket_t fd)
{
    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
}

Http* Http::create(event_base* base, evutil_socket_t fd)
{
    Http* http = new Http(base, fd);
    return http;
}

void Http::strdecode(char *to, char *from)
{
    for (; *from != '\0'; ++to, ++from)
    {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            // 依次判断from中 %20 三个字符
            *to = hexit(from[1]) * 16 + hexit(from[2]);
            from += 2;
        }
        else
        {
            *to = *from;
        }
    }
    *to = '\0';
}

//16进制数转化为10进制, return 0不会出现
int Http::hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

// "编码"，用作回写浏览器的时候，将除字母数字及/_.-~以外的字符转义后回写。
// strencode(encoded_name, sizeof(encoded_name), name);
void Http::strencode(char* to, size_t tosize, const char* from)
{
    size_t tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from)
    {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0)
        {
            *to = *from;
            ++to;
            ++tolen;
        }
        else
        {
            sprintf(to, "%%%02x", (int)*from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}

const char* Http::get_file_type(const char *name)
{
    char names[100];
    strcpy(names,name);

    char* dot;

    dot = strrchr(names, '.');	//自右向左查找‘.’字符;如不存在返回NULL

    if (dot == (char*)0)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}
