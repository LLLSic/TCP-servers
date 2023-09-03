#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //unix standard
#include <string.h>
#include <arpa/inet.h>
#include <ctype.h>

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

    //6.1
    fd_set redset; //6.1.1 读集合
    FD_ZERO(&redset);
    FD_SET(fd, &redset); //6.1.2 将监听的文件描述符给set进去
    int maxfd = fd; //存储最大的文件描述符
    while (1) {
        fd_set tmp = redset;
        int ret = select(maxfd+1, &tmp, NULL, NULL, NULL); //6.1 最大描述符+1、读集合、写集合、异常集合、超时struct。超时设置为NULL，就会一直检测，直到检测到有就绪状态的文件描述符传出
        //6.2 一旦有返回，就说明有读的文件描述符就绪了
        
        if (FD_ISSET(fd, &tmp)) { //6.2.1 判断是不是监听的文件描述符——这时候accept肯定就有数据了，不会阻塞
            //接受客户端的连接
            struct sockaddr_in caddr;
            int addrlen = sizeof(caddr);
            int cfd = accept(fd, (struct sockaddr*)&caddr, &addrlen); //4.1 监听socket、客户端地址信息（传出），长度（传入传出）
            //4.2 这里返回的cfd：用于通信的文件描述符！！
            if (ret == -1) {
                perror("accept");
                continue;  //6.2.2 这里我设置的continue，不成功就会重新继续监听
            }

            FD_SET(cfd, &redset); //6.3 更新文件描述符——把需要通信的加进去
            maxfd = cfd > maxfd ? cfd : maxfd;
            printf("listen, cfd:%d, maxfd:%d.\n", cfd, maxfd);
        }

        for (int cfd = 0; cfd < maxfd+1; cfd++) { //maxfd+1
            if (cfd != fd && FD_ISSET(cfd, &tmp)) { //6.4 用于通信的文件描述符

                //接受数据
                char buff[1024];
                int len = recv(cfd, buff, sizeof(buff), 0);  //5.1 通信文件描述符、接受的数据缓存（传出参数），返回值len表示接受的buff的实际长度--这里read一样的
                // 5.1.0 这里recv会一直阻塞，直到接受到数据（接收到0就是客户端断开）
                if (len > 0) { //5.1.1 长度大于0表示成功
                    printf("client say: %s\n", buff);
                    //小写转大写——增加操作
                    for (int i = 0; i < len; i ++) {
                        buff[i] = toupper(buff[i]);
                    }
                    //返回数据
                    send(cfd, buff, len, 0);  //5.2 通信文件描述符、发送的数据缓存(传入)，实际长度，0默认
                }
                else if (len == 0) { //5.1.2 长度==0表示客户端断开连接
                    printf("client unconnect...\n");
                    //6.4.1 断开连接就需要从redset里面删除掉
                    FD_CLR(cfd, &redset);
                    close(cfd);
                    continue;
                }
                else { //5.1.3 len<0表示接受失败
                    perror("recerve fail...\n");
                    // FD_CLR(cfd, &redset);
                    continue;
                }

            }
        }
    }


    // 关闭文件描述符
    close(fd);

    return 0;
}