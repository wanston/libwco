# libwco
A  C asymmetric coroutine library.

# 介绍
libwco是一个C语言非对称协程库，当前仅支持Sys V ABI x86-64。

近期读了Tencent的协程库[libco](https://github.com/Tencent/libco)的源码，发现其虽然star很多，但是有着不少的缺陷：
* 协程上下文切换函数coctx_swap违反Sys V ABI i386/x86-64。
* hook的系统调用和原系统调用的表现不一致。例如，read新建的socket，既不是阻塞式的无限等待，也不是非阻塞式的立即返回，而是阻塞1秒后返回-1。
* 协程是栈式调度，协程的调度顺序不可控，某些情况下触发bug。

因此，萌生了自己写个协程库的想法，这也是本项目的来源。

本项目部分参考了[libco](https://github.com/Tencent/libco)和[libaco](https://github.com/hnes/libaco)的实现。

# 特性
* 遵守Sys V ABI x86-64
* 可选的共享栈模式
* hook的系统调用和原接口保持完全一致
* 基于epoll的协程调度
* 避免了libco下的已知bug

# Build
```
$ cd /path/to/libwco
$ mkdir build
$ cd build
$ cmake ..
$ make

```

# 使用
具体使用请参考项目中example_*.c文件。
