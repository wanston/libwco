//
// Created by tong on 19-4-2.
//

#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include "wco_hook_sys_call.h"

/*
 * 设计思路：
 * 如果需要支持poll的话，就必须记录一堆信息比较麻烦，所以先不支持poll。
 * hook的效果：
 * 如果是阻塞fd，hook的IO系统调用应该表现为一直阻塞，直到ready然后返回；获取fd的属性也应该表现为O_BLOCK;
 * 如果是非阻塞fd，hook的IO系统调用应该和未hook的系统调用表现一致；获取fd的属性应该表现为O_NONBLOCK。
 *
 * 那么，实现超时断开连接就应该是用户层面的事情了。
 *
 * 为了屏蔽fd的真实属性和fd表现给用户的属性的差异，需要加一个数据结构记录fd的真实属性。
 * socket: 默认是阻塞的IO。
 *
 * read、write，添加timeout到scheduler。
 * */

/*
 * hook的系统调用暂时支持：
 * socket connect accept
 * read write
 * recv send
 * readv writev
 * recvfrom sendto
 * recvmsg sendmsg
 * close
 * fcntl、getsockopt、setsockopt、gethostbyname
 * */

#define HOOK_SYS_FUNC(name) if( !g_sys_##name##_func ) { g_sys_##name##_func = (name##_pfn_t)dlsym(RTLD_NEXT,#name); }

//#define USER_O_NONBLOCK 0
//#define USER_O_BLOCK 1

struct socket_attr_t{
    bool block;
    struct timeval read_timeout;
    struct timeval write_timeout;
};

static struct socket_attr_t *socket_attr_array[102400];

typedef int (*socket_pfn_t)(int domain, int type, int protocol);
typedef int (*connect_pfn_t)(int socket, const struct sockaddr *address, socklen_t address_len);
typedef int (*accept_pfn_t) (int fd, struct sockaddr * addr, socklen_t * addr_len);
typedef int (*close_pfn_t)(int fd);

typedef ssize_t (*read_pfn_t)(int fildes, void *buf, size_t nbyte);
typedef ssize_t (*write_pfn_t)(int fildes, const void *buf, size_t nbyte);

typedef size_t (*send_pfn_t)(int socket, const void *buffer, size_t length, int flags);
typedef ssize_t (*recv_pfn_t)(int socket, void *buffer, size_t length, int flags);

typedef ssize_t (*sendto_pfn_t)(int socket, const void *message, size_t length,
                                int flags, const struct sockaddr *dest_addr,
                                socklen_t dest_len);
typedef ssize_t (*recvfrom_pfn_t)(int socket, void *buffer, size_t length,
                                  int flags, struct sockaddr *address,
                                  socklen_t *address_len);

typedef ssize_t (*recvmsg_pfn_t)(int sockfd, struct msghdr *msg, int flags);
typedef ssize_t (*sendmsg_pfn_t)(int socket, const struct msghdr *message, int flags);

typedef int (*fcntl_pfn_t)(int fildes, int cmd, ...);

typedef struct hostent* (*gethostbyname_pfn_t)(const char *name);

typedef int (*setsockopt_pfn_t)(int socket, int level, int option_name,
                                const void *option_value, socklen_t option_len);


static socket_pfn_t g_sys_socket_func;
static fcntl_pfn_t g_sys_fcntl_func;


int socket(int domain, int type, int protocol){
    HOOK_SYS_FUNC(socket);

    if(!WcoHookIsEnabled()){
        return g_sys_socket_func(domain, type, protocol);
    }

    int fd = g_sys_socket_func(domain,type,protocol);
    if( fd < 0 ) {
        return fd;
    }

    struct socket_attr_t* fd_attr = calloc(1, sizeof(struct socket_attr_t));
    fd_attr->block = true;
    int flags = g_sys_fcntl_func(fd, F_GETFL) | O_NONBLOCK;
    assert(g_sys_fcntl_func( fd, F_SETFL, flags) >= 0);
    socket_attr_array[fd] = fd_attr;
    return fd;
}


int fcntl(int fd, int cmd, ...){
    HOOK_SYS_FUNC( fcntl );

    if(fd < 0){
        return -1;
    }

    va_list arg_ptr;
    va_start(arg_ptr, cmd);
    int ret;
    switch(cmd){
        case F_DUPFD:
        case F_SETFD:
        case F_SETOWN:
        {
            int param = va_arg(arg_ptr,int);
            ret = g_sys_fcntl_func( fd,cmd,param );
            break;
        }
        case F_GETLK:
        case F_SETLK:
        case F_SETLKW:
        {
            struct flock *param = va_arg(arg_ptr,struct flock *);
            ret = g_sys_fcntl_func( fd,cmd,param );
            break;
        }
        case F_GETFD:
        case F_GETOWN:
        {
            ret = g_sys_fcntl_func( fd,cmd );
            break;
        }
        case F_GETFL:
        {
            ret = g_sys_fcntl_func( fd,cmd );
            struct socket_attr_t *fd_attr = socket_attr_array[fd];
            if( WcoHookIsEnabled() && fd_attr){
                ret = fd_attr->block ? (ret&~O_NONBLOCK) : (ret|O_NONBLOCK);
            }
            break;
        }
        case F_SETFL:
        {
            struct socket_attr_t *fd_attr = socket_attr_array[fd];
            int flag = va_arg(arg_ptr,int);
            if( WcoHookIsEnabled() && fd_attr) { // 如果开启了hook，并且fd是socket fd
                fd_attr->block = flag & O_NONBLOCK ? false : true;
                flag |= O_NONBLOCK;
            }
            ret = g_sys_fcntl_func( fd,cmd,flag );
            break;
        }
        default:
            // TODO: 完善所有的情况
            ret = -1;
    }
    va_end(arg_ptr);
    return ret;
}




int connect(int fd, const struct sockaddr *address, socklen_t address_len)
{
    HOOK_SYS_FUNC( connect );

    if( !co_is_enable_sys_hook() )
    {
        return g_sys_connect_func(fd,address,address_len);
    }

    //1.sys call
    int ret = g_sys_connect_func( fd,address,address_len );


    if( O_NONBLOCK & lp->user_flag )
    {
        return ret;
    }

    if (!(ret < 0 && errno == EINPROGRESS))
    {
        return ret;
    }

    //2.wait
    int pollret = 0;
    struct pollfd pf = { 0 };

    for(int i=0;i<3;i++) //25s * 3 = 75s
    {
        memset( &pf,0,sizeof(pf) );
        pf.fd = fd;
        pf.events = ( POLLOUT | POLLERR | POLLHUP );

        pollret = poll( &pf,1,25000 );

        if( 1 == pollret  )
        {
            break;
        }
    }
    if( pf.revents & POLLOUT ) //connect succ
    {
        errno = 0;
        return 0;
    }

    //3.set errno
    int err = 0;
    socklen_t errlen = sizeof(err);
    getsockopt( fd,SOL_SOCKET,SO_ERROR,&err,&errlen);
    if( err )
    {
        errno = err;
    }
    else
    {
        errno = ETIMEDOUT;
    }
    return ret;
}

