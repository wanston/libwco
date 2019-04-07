//
// Created by tong on 19-4-6.
//
#include <sys/socket.h>
#include <assert.h>
#include <netinet/in.h>
#include <memory.h>
#include <stdio.h>
#include <unistd.h>
#include "wco_hook_sys_call.h"
#include "wco_scheduler.h"

void worker_routine(void *arg){
    int fd = (int)arg;
    struct timeval t = {5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(struct timeval));
    char buf[1024];
    ssize_t ret;
    while((ret = read(fd, buf, sizeof(buf))) > 0){
        char c = 0;
        char *r = "HTTP/1.1 404 NOT FOUND!\r\nContent-Length:4\r\n\r\nFUCK";
        int ret = write(fd, r, strlen(r));
        printf("read fd %d\n", fd);
        buf[14] = '\0';
        printf("request %s\n", buf);
    }
    printf("close %d\n", fd);
    close(fd);
}

void accept_routine(void *arg){
    unsigned short port = 8080;

    int listen_fd = socket(AF_INET,SOCK_STREAM, 0);
    int nReuseAddr = 1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&nReuseAddr,sizeof(nReuseAddr));

    struct sockaddr_in server_addr;
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    assert(bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0);

    int max_listen_num = 2048;
    assert(listen(listen_fd, max_listen_num) == 0);

    int fd = 0;
    struct sockaddr_in clientAddr;
    socklen_t clientSize = sizeof(clientAddr);
    while(1){
        if((fd = accept(listen_fd, (struct sockaddr *) &clientAddr, &clientSize)) >= 0) {
            printf("accept success.\n");
            WcoRoutine *co = WcoCreate(NULL, worker_routine, fd);
            printf("create co %x fd %d\n", co, fd);
            WcoAddCoToScheduler(WcoGetScheduler(), co);
        }else{
            printf("accept fail.\n");
        }
    }
}

int main(){
    WcoRoutine *acceptCo = WcoCreate(NULL, accept_routine, NULL);
    printf("acceptCo %x\n", acceptCo);

    WcoSetHookEnabled(true);

    WcoAddCoToScheduler(WcoGetScheduler(), acceptCo);

    WcoRunScheduler(WcoGetScheduler());

    WcoDestroyScheduler(WcoGetScheduler());
}