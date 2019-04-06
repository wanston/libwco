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
#include <sys/epoll.h>
#include <errno.h>
#include <memory.h>
#include "wco_hook_sys_call.h"
#include "wco_scheduler.h"

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

extern int WcoAddEventToScheduler(WcoScheduler* scheduler, WcoRoutine* co, int fd, uint32_t events, struct timeval time_out);
extern void WcoRemoveEventFromScheduler(WcoScheduler* , WcoRoutine*co, int fd, uint32_t events);

struct socket_attr_t{
    bool block;
    struct timeval read_timeout;
    struct timeval write_timeout;
};

static struct socket_attr_t *socket_attr_array[102400];
static __thread bool hook_is_enabled;

typedef int (*socket_pfn_t)(int domain, int type, int protocol);
typedef int (*connect_pfn_t)(int socket, const struct sockaddr *address, socklen_t address_len);
typedef int (*accept_pfn_t) (int fd, struct sockaddr * addr, socklen_t * addr_len);
typedef int (*close_pfn_t)(int fd);

typedef ssize_t (*read_pfn_t)(int fildes, void *buf, size_t nbyte);
typedef ssize_t (*write_pfn_t)(int fildes, const void *buf, size_t nbyte);

typedef ssize_t (*recv_pfn_t)(int socket, void *buffer, size_t length, int flags);
typedef size_t (*send_pfn_t)(int socket, const void *buffer, size_t length, int flags);

typedef ssize_t (*recvfrom_pfn_t)(int socket, void *buffer, size_t length,
                                  int flags, struct sockaddr *address,
                                  socklen_t *address_len);
typedef ssize_t (*sendto_pfn_t)(int socket, const void *message, size_t length,
                                int flags, const struct sockaddr *dest_addr,
                                socklen_t dest_len);

typedef int (*fcntl_pfn_t)(int fildes, int cmd, ...);

typedef int (*getsockopt_pfn_t)(int sockfd, int level, int optname,
                                void *optval, socklen_t *optlen);
typedef int (*setsockopt_pfn_t)(int sockfd, int level, int optname,
                                const void *optval, socklen_t optlen);


static socket_pfn_t g_sys_socket_func;
static fcntl_pfn_t g_sys_fcntl_func;
static connect_pfn_t g_sys_connect_func;
static accept_pfn_t g_sys_accept_func;
static close_pfn_t g_sys_close_func;
static read_pfn_t g_sys_read_func;
static write_pfn_t g_sys_write_func;
static send_pfn_t g_sys_send_func;
static recv_pfn_t g_sys_recv_func;
static sendto_pfn_t g_sys_sendto_func;
static recvfrom_pfn_t g_sys_recvfrom_func;
static getsockopt_pfn_t g_sys_getsockopt_func;
static setsockopt_pfn_t g_sys_setsockopt_func;


static struct socket_attr_t* AllocSocketAttr(int fd){
    struct socket_attr_t* fd_attr = calloc(1, sizeof(struct socket_attr_t));
    assert(fd_attr);
    fd_attr->block = true;
    HOOK_SYS_FUNC(fcntl);
    int flags = g_sys_fcntl_func(fd, F_GETFL) | O_NONBLOCK;
    assert(g_sys_fcntl_func( fd, F_SETFL, flags) >= 0);
    return fd_attr;
}


static void FreeSocketAttr(int fd){
    free(socket_attr_array[fd]);
    socket_attr_array[fd] = NULL;
}


int socket(int domain, int type, int protocol){
    HOOK_SYS_FUNC(socket);

    if(!WcoIsHookEnabled()){
        return g_sys_socket_func(domain, type, protocol);
    }

    int fd = g_sys_socket_func(domain,type,protocol);
    if( fd < 0 ) {
        return fd;
    }

    socket_attr_array[fd] = AllocSocketAttr(fd);
    return fd;
}


int connect(int fd, const struct sockaddr *address, socklen_t address_len){
    HOOK_SYS_FUNC(connect);

    if(WcoIsHookEnabled() && socket_attr_array[fd] && socket_attr_array[fd]->block){
        int ret = g_sys_connect_func(fd, address, address_len);
        if(ret < 0 &&  errno == EAGAIN){ // 存疑
            struct timeval t = {0,0};
            WcoAddEventToScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLOUT, t);
            WcoYield();
            WcoRemoveEventFromScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLOUT);
            ret = g_sys_connect_func(fd, address, address_len);
        }

        if(ret < 0 && errno == EINPROGRESS){
            struct timeval t = {0,0};
            WcoAddEventToScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLOUT, t);
            WcoYield();
            WcoRemoveEventFromScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLOUT);
            // TODO: 需要渠道得知 是超时，还是某个事件发生导致协程醒来
            int err = 0;
            socklen_t errlen = sizeof(err);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
            errno = err;
            ret = err ? -1 : 0;
        }
        return ret;
    }else{
        return g_sys_connect_func(fd, address, address_len);
    }
}


int accept(int fd, struct sockaddr * addr, socklen_t * addr_len){
    HOOK_SYS_FUNC(accept);

    if(WcoIsHookEnabled() && socket_attr_array[fd] && socket_attr_array[fd]->block){
        int ret = g_sys_accept_func(fd, addr, addr_len);
        if(ret < 0 && errno == EAGAIN){
            struct timeval t = {0,0};
            WcoAddEventToScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLIN, t);
            WcoYield();
            WcoRemoveEventFromScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLIN);
            ret = g_sys_accept_func(fd, addr, addr_len);
            if(ret >= 0){
                socket_attr_array[fd] = AllocSocketAttr(fd); // Linux下，accept不会继承listen_fd的flags。
            }
        }
        return ret;
    }
    return g_sys_accept_func(fd, addr, addr_len);
}


int close(int fd)
{
    HOOK_SYS_FUNC(close);

    if(socket_attr_array[fd]){ // 扩大判断范围
        FreeSocketAttr(fd);
    }
    return g_sys_close_func(fd);
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
            if( WcoIsHookEnabled() && fd_attr){
                ret = fd_attr->block ? (ret&~O_NONBLOCK) : (ret|O_NONBLOCK);
            }
            break;
        }
        case F_SETFL:
        {
            struct socket_attr_t *fd_attr = socket_attr_array[fd];
            int flag = va_arg(arg_ptr,int);
            if( WcoIsHookEnabled() && fd_attr) { // 如果开启了hook，并且fd是socket fd
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


ssize_t read(int fd, void *buf, size_t nbyte ){
    HOOK_SYS_FUNC(read);

    if(WcoIsHookEnabled() && socket_attr_array[fd] && socket_attr_array[fd]->block){
        ssize_t ret = g_sys_read_func(fd, buf, nbyte);
        if(ret < 0 && errno == EAGAIN){
            WcoAddEventToScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLIN, socket_attr_array[fd]->read_timeout);
            WcoYield();
            WcoRemoveEventFromScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLIN);
            // 要么因为可读而返回；要么不可读，因为超时而返回。
            ret = g_sys_read_func(fd, buf, nbyte);
        }
        return ret;
    }

    return g_sys_read_func(fd, buf, nbyte);
}


ssize_t write(int fd, const void *buf, size_t nbyte ){
    HOOK_SYS_FUNC(write);

    if(WcoIsHookEnabled() && socket_attr_array[fd] && socket_attr_array[fd]->block){
        ssize_t ret = g_sys_write_func(fd, buf, nbyte);
        if(ret < 0 && errno == EAGAIN){
            WcoAddEventToScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLOUT, socket_attr_array[fd]->write_timeout);
            WcoYield();
            WcoRemoveEventFromScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLOUT);
            // 要么因为可写而返回；要么不可写，因为超时而返回。
            ret = g_sys_write_func(fd, buf, nbyte);
        }
        return ret;
    }

    return g_sys_write_func(fd, buf, nbyte);
}


ssize_t recv( int fd, void *buf, size_t n, int flags ){
    HOOK_SYS_FUNC(recv);

    if(WcoIsHookEnabled() && socket_attr_array[fd] && socket_attr_array[fd]->block){
        ssize_t ret = g_sys_recv_func(fd, buf, n, flags);
        if(ret < 0 && errno == EAGAIN){
            WcoAddEventToScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLIN, socket_attr_array[fd]->read_timeout);
            WcoYield();
            WcoRemoveEventFromScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLIN);
            // 要么因为可读而返回；要么不可读，因为超时而返回。
            ret = g_sys_recv_func(fd, buf, n, flags);
        }
        return ret;
    }

    return g_sys_recv_func(fd, buf, n, flags);
}


ssize_t send(int fd, const void *buf, size_t n, int flags){
    HOOK_SYS_FUNC(send);

    if(WcoIsHookEnabled() && socket_attr_array[fd] && socket_attr_array[fd]->block){
        ssize_t ret = g_sys_send_func(fd, buf, n, flags);
        if(ret < 0 && errno == EAGAIN){
            WcoAddEventToScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLOUT, socket_attr_array[fd]->write_timeout);
            WcoYield();
            WcoRemoveEventFromScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLOUT);
            // 要么因为可写而返回；要么不可写，因为超时而返回。
            ret = g_sys_send_func(fd, buf, n, flags);
        }
        return ret;
    }

    return g_sys_send_func(fd, buf, n, flags);
}


ssize_t recvfrom(int fd, void *buf, size_t n,
                 int flags, struct sockaddr *addr,
                 socklen_t *addr_len)
{
    HOOK_SYS_FUNC(recvfrom);

    if(WcoIsHookEnabled() && socket_attr_array[fd] && socket_attr_array[fd]->block){
        ssize_t ret = g_sys_recvfrom_func(fd, buf, n, flags, addr, addr_len);
        if(ret < 0 && errno == EAGAIN){
            WcoAddEventToScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLIN, socket_attr_array[fd]->read_timeout);
            WcoYield();
            WcoRemoveEventFromScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLIN);
            // 要么因为可读而返回；要么不可读，因为超时而返回。
            ret = g_sys_recvfrom_func(fd, buf, n, flags, addr, addr_len);
        }
        return ret;
    }

    return g_sys_recvfrom_func(fd, buf, n, flags, addr, addr_len);
}


ssize_t sendto(int fd, const void *buf, size_t n,
               int flags, const struct sockaddr *addr,
               socklen_t addr_len)
{
    HOOK_SYS_FUNC(sendto);

    if(WcoIsHookEnabled() && socket_attr_array[fd] && socket_attr_array[fd]->block){
        ssize_t ret = g_sys_sendto_func(fd, buf, n, flags, addr, addr_len);
        if(ret < 0 && errno == EAGAIN){
            WcoAddEventToScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLOUT, socket_attr_array[fd]->write_timeout);
            WcoYield();
            WcoRemoveEventFromScheduler(WcoGetScheduler(), WcoGetCurrentCo(), fd, EPOLLOUT);
            // 要么因为可写而返回；要么不可写，因为超时而返回。
            ret = g_sys_sendto_func(fd, buf, n, flags, addr, addr_len);
        }
        return ret;
    }

    return g_sys_sendto_func(fd, buf, n, flags, addr, addr_len);
}


extern int getsockopt (int fd, int level, int optname,
                       void *optval, socklen_t *optlen)
{
    HOOK_SYS_FUNC(getsockopt);

    if(WcoIsHookEnabled() && socket_attr_array[fd] && level == SOL_SOCKET){
        if(optname == SO_RCVTIMEO)
        {
            *(struct timeval*)optval = socket_attr_array[fd]->read_timeout;
            return 0;
        }
        else if(optname == SO_SNDTIMEO)
        {
            *(struct timeval*)optval = socket_attr_array[fd]->write_timeout;
            return 0;
        }
    }
    return g_sys_getsockopt_func(fd, level, optname, optval, optlen);
}


int setsockopt(int fd, int level, int optname,
               const void *optval, socklen_t optlen)
{
    HOOK_SYS_FUNC(setsockopt);

    if(WcoIsHookEnabled() && socket_attr_array[fd] && level == SOL_SOCKET){
        if(optname == SO_RCVTIMEO)
        {
            socket_attr_array[fd]->read_timeout = *(struct timeval *) optval;
            return 0;
        }
        else if(optname == SO_SNDTIMEO)
        {
            socket_attr_array[fd]->write_timeout = *(struct timeval*)optval;
            return 0;
        }
    }

    return g_sys_setsockopt_func(fd, level, optname, optval, optlen);
}


bool WcoIsHookEnabled(){
    return hook_is_enabled;
}


void WcoSetHookEnabled(bool b){
    hook_is_enabled = b;
}