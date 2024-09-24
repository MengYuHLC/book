#ifndef _COMMENT_H
#define _COMMENT_H
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sqlite3.h>
#include <stdlib.h>
#define MAX_EVENTS 100
typedef struct reigster_node
{
    char name[128];
    char password[128];
} REG_NODE;

typedef struct add_friend
{
    char firend_name[128];
    char self_name[128];
} ADD_FRI;

typedef struct chat
{
    char firend_name[128];
    char self_name[128];
    char text[512];
} CHAT;

typedef struct message_node 
{
    short type;      // 消息的类型，1.注册 2.加好友 3.私聊 4.群聊 5......
    char buf[1022]; // 存放各种消息的数据
    char name[128];
} MSG;

typedef struct loginInfo//登录信息的结构体
{
    char name[128];
    int fd;
}LOGIN_INFO;

typedef struct singleChatNode//单聊用的结构体
{
    char destName[128];
    char selfName[128];
    char text[700];
}SIG_CHAT;

char * intToStr(unsigned int p)
{
    char *ip = malloc(16);
    inet_ntop(AF_INET, &p, ip, 16);
    return ip;
}

int strToInt(char *ip)
{
    unsigned int p;
    inet_pton(AF_INET, ip, &p);
    return p;
}

#endif
