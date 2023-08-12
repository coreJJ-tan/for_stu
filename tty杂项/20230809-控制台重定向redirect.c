redirect 定义在 drivers/tty/tty_io.c 中，修改 direct 的函数只有 tioccons ，用于相应 tty ioctl 的 TIOCCONS 命令。
根据 man tty_ioctl 的输出，这个命令的功能为：
重定向控制台输出，将原本要输出到 /dev/console 或者 /dev/tty0 的内容重定向到给定的终端。如果终端是一个伪终端的主设备，将其发送到从设备。

2.6.10 之前内核只要输出没有重定向，任何用户都可以执行这个操作； 2.6.10 版本开始只有 CAP_SYS_ADMIN 进程可以执行这个操作。如果输出已经进行了重定向，返回 EBUSY ；可以通过传入 /dev/console 或者 /dev/tty0 结束重定向。