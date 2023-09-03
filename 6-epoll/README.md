# 6-epoll

- 使用epoll实现多路复用
  - `server.c`：在简单socket的基础上使用epoll实现IO多路复用，默认使用的LT模式
  - `server_LT.c`、`server_ET.c`：在`server.c`的基础上，减少缓存容量，用于验证LT\ET模式下出现的情况差异
  - `server_ET_nonblock.c`：ET模式更改为非阻塞模式
  - `server_ET_nonblock_multy.c`：增加多线程的使用
- 前置步骤：
  - socket：https://github.com/LLLSic/TCP-servers/tree/main/1-socket
- 可以按照注释顺序注意一些注意点

