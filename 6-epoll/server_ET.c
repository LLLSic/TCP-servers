#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //unix standard
#include <string.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/epoll.h> //8.1 epoll
#include <fcntl.h> //9.5 法三，f-control的头文件
#include <errno.h>

int main() {
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

    //8.2 创建epoll实例
    int epfd = epoll_create(100); //参数>0即可
    if (epfd == -1) {
        //失败了
        perror("epoll_create");
        exit(0);
    }
    struct epoll_event ev; //8.3.1委托事件
    ev.events = EPOLLIN | EPOLLET; //9.2 设定边沿模式——监听的fd可以不改
    ev.data.fd = fd;  //8.3.2 一般没有特殊情况就是fd，说明是该fd调用的
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev); //8.3 将监听套接字添加到epoll中，epoll、op、fd、委托epoll的事件
    //8.3.3 epoll_ctl也有返回值，这里不判断了（-1）
    
    struct epoll_event evs[1024];
    int size = sizeof(evs)/sizeof(evs[0]);

    //6.1
    fd_set redset; //6.1.1 读集合
    FD_ZERO(&redset);
    FD_SET(fd, &redset); //6.1.2 将监听的文件描述符给set进去
    int maxfd = fd; //存储最大的文件描述符
    while (1) {
        int num = epoll_wait(epfd, evs, size, -1); //8.4 阻塞（-1）等待
        printf("num = %d\n", num);
        for (int i = 0; i < num; i++) {
            int evfd = evs[i].data.fd; //8.4.1 该fd就是触发的fd
            if (evfd == fd) { //8.4.2 说明是监听的fd
                //接受客户端的连接
                // printf("接受客户端的连接\n");
                struct sockaddr_in caddr;
                int addrlen = sizeof(caddr);
                int cfd = accept(fd, (struct sockaddr*)&caddr, &addrlen); //4.1 监听socket、客户端地址信息（传出），长度（传入传出）
                
                //9.5.1 方法三设置fd的非阻塞属性
                int flag = fcnl(cfd, F_GETFL);
                flag |= 0_NONBLOCK; 
                fcntl(cfd, F_SETFL, flag);

                //4.2 这里返回的cfd：用于通信的文件描述符
                if (ret == -1) {
                    perror("accept");
                    continue;  //6.2.2 这里我设置的continue，不成功就会重新继续监听
                }

                //添加通信cfd
                ev.events = EPOLLIN | EPOLLET; //9.2.1 设置通信fd为边沿触发模式
                ev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
            } else {
                //接受数据
                    char buff[5]; //9.1 一次性最多五个字节，默认LT-法一：增加buff
                while (1) { //9.3 在ET的情况下
                    //9.4 方法二，一直读取——单线程会阻塞——要求recv不能阻塞
                    int len = recv(evfd, buff, sizeof(buff), 0);  //5.1 通信文件描述符、接受的数据缓存（传出参数），返回值len表示接受的buff的实际长度--这里read一样的
                    // 5.1.0 这里recv会一直阻塞，直到接受到数据（接收到0就是客户端断开）
                    if (len > 0) { //5.1.1 长度大于0表示成功
                        printf("client say: %s\n", buff);
                        //小写转大写——增加操作
                        for (int i = 0; i < len; i ++) {
                            buff[i] = toupper(buff[i]);
                        }
                        write(STDOUT, buff, len); //
                        //返回数据
                        send(evfd, buff, len, 0);  //5.2 通信文件描述符、发送的数据缓存(传入，不会被修改)，实际长度，0默认
                        //可以加一个判断send
                    }
                    else if (len == -1 && errno == EAGAIN) { //读完了
                        pringtf("数据接受完毕");
                    }
                    else if (len == 0) { //5.1.2 长度==0表示客户端断开连接
                        printf("client unconnect...\n");
                        //6.4.1 断开连接
                        epoll_ctl(epfd, EPOLL_CTL_DEL, evfd, NULL);
                        close(evfd);  //8.6先删除，再关闭（不然会返回-1）
                        continue;
                    }
                    else { //5.1.3 len<0表示接受失败
                        perror("recerve fail...\n");
                        continue;
                    }
                }
                
            }
        }
    }


    // 关闭文件描述符
    close(fd);

    return 0;
}