B-TCP
================

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;这是一个由斯坦福大学CS144课程重构的项目，从文件描述符开始一步步自底向上构建出B_TCPSocket。其API命名部分沿用了<sys/socket.h>中的socket，因此您可以很方便地利用B_TCPSocket编程，甚至完全替代Linux中的socket。  

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;除此之外，我们重写了TCP/IP协议栈，实现了流量控制，拥塞控制等需求，尽可能地还原目前的TCP协议。  

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;您可能发现该项目与CS144项目有很大不同，具体来说，我们重新实现了TCPEndPoint的架构，并且将poll机制改为epoll机制，未来我们可能还会实现并行通信和负载均衡。

**更多内容请参阅[项目文档](https://docs.qq.com/doc/p/3ea242d023d67e2f4535da688e90a875fbdb46e1?u=020c8967a2ec480b83f6d664776800d1)**