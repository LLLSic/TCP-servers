#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int main() {
    //1.创建通信的套接字，和服务器一样
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    //6.连接服务器
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(9999); //6.1 地址族协议（ipv4）和端口和服务器端都一样
    // saddr.sin_addr.s_addr = INADDR_ANY; //6.2 需要对应到服务器端的ip地址（ipconfig）,使用下面的pton转换并存储在saddr.sin_addr.s_addr中
    inet_pton(AF_INET, "192.168.16.129", &saddr.sin_addr.s_addr);

    int ret = connect(fd, (struct sockaddr*)&saddr, sizeof(saddr)); //6.3 socket、本地ip
    if (ret == -1) {
        perror("connect");
        return -1;
    }

    // 7.通信——客户端发送，后面接受
    int number = 0;
    while(1) {
        // 7.1 发送数据
        char buff[1024];
        sprintf(buff, "hello world, %d..\n", number++);
        send(fd, buff, strlen(buff)+1, 0);  //7.1.1 通信文件描述符、发送的数据、发送的实际长度，返回值说s表示实际真正发送的buff的长度--这里write一样的
        //这里的strlen(buff)+1：加上最后的\0，后面也可以加上send的返回值判断

        // 7.2 接受数据
        memset(buff, 0, sizeof(buff)); // 7.2.1 先把缓存置为0，后用来接收
        int len = recv(fd, buff, sizeof(buff), 0);
        if (len > 0) { 
            printf("sever say: %s\n", buff);
        }
        else if (len == 0) {
            printf("sever unconnect...\n");  //7.2.2 这里0表示服务器断开连接
            break;
        }
        else {
            perror("client recerve fail...\n");
            break;
        }
        sleep(2); // 重复发送接受
    }

    // 关闭文件描述符
    close(fd);

    return 0;
}