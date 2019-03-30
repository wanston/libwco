1. 基于协程的服务器思路
写服务器，用多个线程。每个线程内容相同。

思路一：
每个线程中有1个accept协程，该协程一直accept，把fd添加到线程局部存储中，直到EAGAIN；EAGAIN后，yield协程，控制权交给主协程。
主协程中，负责新建协程，resume协程。但是，这又和eventloop产生了冲突，需要入侵eventloop源码，在每个loop中新建协程，resume协程。

思路二：
每个线程中有1个accept协程，该协程一直accept，每成功accept一次，就新建协程，resume协程。
新的协程yield回accept协程后，会继续accept，直到accept遇到EAGAIN才会yield回主协程。
主协程中运行epoll事件循环，哪个fd活跃就运行哪些协程。
这样做也有不好。如果accept一直可读，那么就无法resume业务协程。导致accept协程的优先级很高，生成一堆执行到一半待resume的协程。


思路三：
在思路二的基础上，accept协程-->worker协程-->accept协程 后，直接yield到主协程，再让主协程负责子协程的调度。
这样做也不好。这样做的前提是accept协程中accept的fd已经添加进了epoll，如果accept未遇到过EAGAIN，就yield回主协程，那么主协程中的epoll永远不会resume accept协程。


思路四：
原理上的思路一，即新写一个“添加协程到eventloop”的api。在eventloop中，epollwait后，resume新添加的协程，resume活跃fd关联的协程。
决定采用思路四。


2. 协程库的API
因此协程库需要实现的api有：
creat()
resume()
yield()
event_loop()
add_co_to_loop()

hook一批系统调用：
read
write
send
recv
accept


libco的resume，可以从任意的协程发起；yield是退回上一个运行的协程。
libco的resume，只能从主协程发起；yield退回主协程。

3. 自己对libco的改进

修复libco中的不遵循x86 Sys V ABI的bug，为协程栈添加保护区。改进hook系统调用。精简api。

关键是子协程中，能否在有eventloop的情况下，新建并运行协程。
答：可以，accept协程中，新建并运行worker协程a，worker协程a遇到hook的IO后，挂起；之后，回到accept协程执行；之后，worker协程a再次执行，都是从主协程切过去的。



resume可以从任何协程发起，yield只能回到主协程，怎么样？不合理！


libaco如果写server，就是accept协程里面，运行库。



应该写pthread风格的协程库。
关于栈共享的问题，实现共享栈。


先实现非共享



