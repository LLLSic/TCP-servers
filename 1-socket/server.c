#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //unix standard
#include <string.h>
#include <arpa/inet.h>  //这里面就已经包含了socket.h，所以套接字就只用记住这个
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <sys/types.h>

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

    //4. 阻塞并等待客户端的链接
    struct sockaddr_in caddr;
    int addrlen = sizeof(caddr);
    int cfd = accept(fd, (struct sockaddr*)&caddr, &addrlen); //4.1 监听socket、客户端地址信息（传出），长度（传入传出）
    //4.2 这里返回的cfd：用于通信的文件描述符
    if (ret == -1) {
        perror("accept");
        return -1;
    }

    //连接建立成功，可以打印一下客户端的IP和端口信息
    char ip[32];
    printf("client's IP: %s, port: %d\n", 
            inet_ntop(AF_INET, &caddr.sin_addr.s_addr, ip, sizeof(ip)),
            ntohs(caddr.sin_port));

    // 5.通信——这里只能接受一个客户端的链接，单线程
    while(1) {
        // 接受数据
        char buff[1024];
        int len = recv(cfd, buff, sizeof(buff), 0);  //5.1 通信文件描述符、接受的数据缓存（传出参数），返回值len表示接受的buff的实际长度--这里read一样的
        // 5.1.0 这里recv会一直阻塞，直到接受到数据（接收到0就是客户端断开）
        if (len > 0) { //5.1.1 长度大于0表示成功
            printf("client say: %s\n", buff);
            //返回数据
            send(cfd, buff, len, 0);  //5.2 通信文件描述符、发送的数据缓存(传入，不会被修改)，实际长度，0默认
            //可以加一个判断send
        }
        else if (len == 0) { //5.1.2 长度==0表示客户端断开连接
            printf("client unconnect...\n");
            break;
        }
        else { //5.1.3 len<0表示接受失败
            perror("recerve fail...\n");
            break;
        }

    }

    // 关闭文件描述符
    close(fd);
    close(cfd);

    return 0;
}