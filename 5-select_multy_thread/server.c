#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //unix standard
#include <string.h>
#include <arpa/inet.h>  //这里面就已经包含了socket.h，所以套接字就只用记住这个
#include <ctype.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <sys/types.h>
#include <pthread.h>
#include <sys/select.h> //7.0 多线程

pthread_mutex_t mutex;  //8.0 加锁，对应rdset和maxfd

typedef struct fdinfo {  //7.1.1 参数传递
    int fd;
    int * maxfd; //因为maxfd需要更新，所以传地址
    fd_set * rdset;
} FDInfo;

void * acceptConn(void *arg) {
    //连接客户端
    FDInfo * info = (FDInfo*)arg;

    struct sockaddr_in caddr;
    int addrlen = sizeof(caddr);
    pthread_mutex_lock(&mutex);
    int cfd = accept(info->fd, (struct sockaddr*)&caddr, &addrlen); //4.1 监听socket、客户端地址信息（传出），长度（传入传出）
    //4.2 这里返回的cfd：用于通信的文件描述符！！
    if (cfd == -1) {
        pthread_mutex_unlock(&mutex);
        perror("accept");
        free(info);
        return NULL;
    }

    FD_SET(cfd, info->rdset); //6.3 更新文件描述符——把需要通信的加进去  //7.1.2 这里就需要加锁
    *info->maxfd = cfd > *info->maxfd ? cfd : *info->maxfd;  //7.1.3 这里要解引用
    pthread_mutex_unlock(&mutex);
    printf("listen, client fd:%d, maxfd:%d, threadID:%ld.\n", cfd, *info->maxfd, pthread_self());

    free(info);
    return NULL;
}

void * communication(void *arg) {
    //接受数据
    FDInfo * info = (FDInfo*)arg;
    char buff[1024];
    int len = recv(info->fd, buff, sizeof(buff), 0);  //5.1 通信文件描述符、接受的数据缓存（传出参数），返回值len表示接受的buff的实际长度--这里read一样的
    // 5.1.0 这里recv会一直阻塞，直到接受到数据（接收到0就是客户端断开）
    if (len > 0) { //5.1.1 长度大于0表示成功
        printf("client say: %s\n", buff);
        //小写转大写——增加操作
        for (int i = 0; i < len; i ++) {
            buff[i] = toupper(buff[i]);
        }
        //返回数据
        send(info->fd, buff, len, 0);  //5.2 通信文件描述符、发送的数据缓存(传入，不会被修改)，实际长度，0默认
        //可以加一个判断send
    }
    else if (len == 0) { //5.1.2 长度==0表示客户端断开连接
        printf("client unconnect...\n");
        //6.4.1 断开连接就需要从redset里面删除掉
        pthread_mutex_lock(&mutex);
        FD_CLR(info->fd, info->rdset);
        pthread_mutex_unlock(&mutex);
        close(info->fd);
        free(info);
        return NULL;
    }
    else { //5.1.3 len<0表示接受失败
        free(info);
        return NULL;
        perror("recerve fail...\n");
        // FD_CLR(cfd, &rdset);
    }
    printf("communication, client fd:%d, threadID:%ld.\n", info->fd, pthread_self());
    free(info);
    return NULL;
}

int main() {
    pthread_mutex_init(&mutex, NULL);
    //1.创建监听的套接字
    int fd = socket(AF_INET, SOCK_STREAM, 0); //1.1 ipv4，流式协议，默认tcp
    if (fd == -1) { //1.2 失败都是返回-1
        perror("socket"); //1.3 perror: print a system error message
        return -1;
    }

    //2.绑定本地的IP port
    struct sockaddr_in saddr; //2.2 IP端口信息，使用sockaddr_in结构，再强制类型转换——这个信息需要转换大小端（字节序）
    saddr.sin_family = AF_INET; //2.2.1 地址族协议，选择ipv4
    saddr.sin_port = htons(9999);  //2.2.2 端口，选一个未被使用的，5000以上基本上就可以满足，htons：host to net short
    saddr.sin_addr.s_addr = INADDR_ANY; //2.2.3 ip地址，0 = 0.0.0.0 对于0来说，大端和小端没有区别，所以不需要转换。INADDR_ANY可以绑定本地任意ip地址，就会绑定本地网卡实际ip地址
    
    int ret = bind(fd, (struct sockaddr*)&saddr, sizeof(saddr)); //2.1 socket、本地ip
    if (ret == -1) {
        perror("bind");
        return -1;
    }

    //3. 设置监听
    ret = listen(fd, 128); //3.1 socekt，最大连接请求
    if (ret == -1) {
        perror("listen");
        return -1;
    }

    //6.1
    fd_set rdset; //6.1.1 读集合  //8.1 这里还不用加，只有一个线程
    FD_ZERO(&rdset);
    FD_SET(fd, &rdset); //6.1.2 将监听的文件描述符给set进去
    int maxfd = fd; //存储最大的文件描述符
    while (1) {
        pthread_mutex_lock(&mutex);
        fd_set tmp = rdset;
        pthread_mutex_unlock(&mutex);
        int ret = select(maxfd+1, &tmp, NULL, NULL, NULL); //6.1 最大描述符+1、读集合、写集合、异常集合、超时struct。超时设置为NULL，就会一直检测，直到检测到有就绪状态的文件描述符传出
        //6.2 一旦有返回，就说明有读的文件描述符就绪了
        
        if (FD_ISSET(fd, &tmp)) { //6.2.1 判断是不是监听的文件描述符——这时候accept肯定就有数据了，不会阻塞
            printf("new listen, fd:%d, maxfd:%d\n", fd, maxfd);
            //7.1 接受客户端的连接
            pthread_t tid;
            FDInfo* info = (FDInfo*)malloc(sizeof(FDInfo));
            info->fd = fd;
            info->maxfd = &maxfd;
            info->rdset = &rdset;
            pthread_create(&tid, NULL, acceptConn, info);
            pthread_detach(tid);
        }

        for (int cfd = 0; cfd < maxfd+1; cfd++) { //maxfd+1!!
            if (cfd != fd && FD_ISSET(cfd, &tmp)) { //6.4 用于通信的文件描述符
                printf("new commu, fd:%d, maxfd:%d\n", fd, maxfd);
                //接受数据
                pthread_t tid;
                FDInfo* info = (FDInfo*)malloc(sizeof(FDInfo));
                info->fd = cfd;
                // info->maxfd = &maxfd;
                info->rdset = &rdset;
                pthread_create(&tid, NULL, communication, info);
                pthread_detach(tid);
            }
        }
    }

    

    // //连接建立成功，可以打印一下客户端的IP和端口信息
    // char ip[32];
    // printf("client's IP: %s, port: %d\n", 
    //         inet_ntop(AF_INET, &caddr.sin_addr.s_addr, ip, sizeof(ip)),
    //         ntohs(caddr.sin_port));


    // 关闭文件描述符
    close(fd);
    pthread_mutex_destroy(&mutex);
    return 0;
}