1, 应用层打开 tty 设备节点都会被关联一个 tty_struct, 这个结构体中会关联操作 tty 设备的所有信息.
    那具体会关联那个 tty_struct  呢?

2, tty_driver 和 tty_struct 在初始化阶段没有被关联起来？