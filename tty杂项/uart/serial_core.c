EXPORT_SYMBOL(uart_update_timeout); // 更新 FIFO 超时时间
EXPORT_SYMBOL(uart_get_baud_rate); // 获取波特率
EXPORT_SYMBOL(uart_get_divisor); // 获取时钟分频系数

#if defined(CONFIG_SERIAL_CORE_CONSOLE) || defined(CONFIG_CONSOLE_POLL)
EXPORT_SYMBOL_GPL(uart_console_write);
EXPORT_SYMBOL_GPL(uart_parse_earlycon);
EXPORT_SYMBOL_GPL(uart_parse_options);
EXPORT_SYMBOL_GPL(uart_set_options);
#endif /* CONFIG_SERIAL_CORE_CONSOLE */

EXPORT_SYMBOL(uart_match_port); // 判断形参的两个端口是否是同一个
EXPORT_SYMBOL_GPL(uart_handle_dcd_change);
EXPORT_SYMBOL_GPL(uart_handle_cts_change);

EXPORT_SYMBOL_GPL(uart_insert_char); // 提交一个字符
EXPORT_SYMBOL(uart_write_wakeup); // 唤醒使用这个串口的程序，可以发送数据到循环缓冲区
EXPORT_SYMBOL(uart_register_driver); // 注册一个 uart_driver
EXPORT_SYMBOL(uart_unregister_driver); // 注销一个 uart_driver
EXPORT_SYMBOL(uart_suspend_port);	// 暂停使用端口
EXPORT_SYMBOL(uart_resume_port);	// 继续使用端口
EXPORT_SYMBOL(uart_add_one_port); // 添加一个 uart_port
EXPORT_SYMBOL(uart_remove_one_port); // 移除一个 uart_port


int uart_register_driver(struct uart_driver *drv)
{
	struct tty_driver *normal;
	int i, retval;

	BUG_ON(drv->state);

	/*
	 * Maybe we should be using a slab cache for this, especially if
	 * we have a large number of ports to handle.
	 */
	drv->state = kzalloc(sizeof(struct uart_state) * drv->nr, GFP_KERNEL); // 申请 drv->nr 数量的 uart_state（一个端口对应一个 uart_state？）
	if (!drv->state)
		goto out;

	normal = alloc_tty_driver(drv->nr);  // 申请一个 tty_driver, 用 uart_driver->nr 作为参数是因为要申请 uart_driver->nr 个 tty_struct
	if (!normal)
		goto out_kfree;

	drv->tty_driver = normal;

	normal->driver_name	= drv->driver_name;
	normal->name		= drv->dev_name;
	normal->major		= drv->major;
	normal->minor_start	= drv->minor;
	normal->type		= TTY_DRIVER_TYPE_SERIAL; // tty_driver 驱动大类
	normal->subtype		= SERIAL_TYPE_NORMAL; // tty_driver 驱动小类
	normal->init_termios	= tty_std_termios; // 设置默认的 ktermios
	normal->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	normal->init_termios.c_ispeed = normal->init_termios.c_ospeed = 9600;
	normal->flags		= TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	normal->driver_state    = drv;      // 将 uart_driver 绑定到 tty_driver （也就是绑定一个 low level driver）
	tty_set_operations(normal, &uart_ops); // 设置 tty_operations

	/*
	 * Initialise the UART state(s).
	 */
	for (i = 0; i < drv->nr; i++) {
		struct uart_state *state = drv->state + i;
		struct tty_port *port = &state->port;  // 一个 uart_state 对应一个 tty_port

		tty_port_init(port); // 初始化一个 tty_port
		port->ops = &uart_port_ops; // 设置 tty_port_operations
	}

	retval = tty_register_driver(normal); // 初始化 tty_driver，申请设备号，并将 tty_driver 挂到全局链表 tty_drivers 中，用户空间可在 /proc/tty/driver 文件查看有哪些 tty_driver
	if (retval >= 0)
		return retval;

	for (i = 0; i < drv->nr; i++)
		tty_port_destroy(&drv->state[i].port);
	put_tty_driver(normal);
out_kfree:
	kfree(drv->state);
out:
	return -ENOMEM;
}
int uart_add_one_port(struct uart_driver *drv, struct uart_port *uport)
{
	struct uart_state *state;
	struct tty_port *port;
	int ret = 0;
	struct device *tty_dev;
	int num_groups;

	BUG_ON(in_interrupt());

	if (uport->line >= drv->nr)
		return -EINVAL;

	state = drv->state + uport->line;
	port = &state->port;

	mutex_lock(&port_mutex);
	mutex_lock(&port->mutex);
	if (state->uart_port) {
		ret = -EINVAL;
		goto out;
	}

	/* Link the port to the driver state table and vice versa */
	state->uart_port = uport; // 将 uart_port 绑定到 uart_state
	uport->state = state; // 指定 uart_port 从属于哪个 uart_state

	state->pm_state = UART_PM_STATE_UNDEFINED;
	uport->cons = drv->cons;
	uport->minor = drv->tty_driver->minor_start + uport->line;

	/*
	 * If this port is a console, then the spinlock is already
	 * initialised.
	 */
	if (!(uart_console(uport) && (uport->cons->flags & CON_ENABLED))) {
		spin_lock_init(&uport->lock);
		lockdep_set_class(&uport->lock, &port_lock_key);
	}
	if (uport->cons && uport->dev)
		of_console_check(uport->dev->of_node, uport->cons->name, uport->line);

	uart_configure_port(drv, state, uport);

	num_groups = 2;
	if (uport->attr_group)
		num_groups++;

	uport->tty_groups = kcalloc(num_groups, sizeof(*uport->tty_groups),
				    GFP_KERNEL);
	if (!uport->tty_groups) {
		ret = -ENOMEM;
		goto out;
	}
	uport->tty_groups[0] = &tty_dev_attr_group;
	if (uport->attr_group)
		uport->tty_groups[1] = uport->attr_group;

	/*
	 * Register the port whether it's detected or not.  This allows
	 * setserial to be used to alter this port's parameters.
	 */
	tty_dev = struct device *tty_port_register_device_attr(struct tty_port *port, struct tty_driver *driver = drv->tty_driver, unsigned index = uport->line,
				struct device *device = uport->dev, void *drvdata = port, const struct attribute_group **attr_grp = uport->tty_groups); // 注册相应的字符设备和 sysfs 下面的设备
		tty_port_link_device(port, driver, index);
		return tty_register_device_attr(driver = drv->tty_driver, index = uport->line, device = uport->dev, drvdata = port, attr_grp = uport->tty_groups);
	if (likely(!IS_ERR(tty_dev))) {
		device_set_wakeup_capable(tty_dev, 1);
	} else {
		dev_err(uport->dev, "Cannot register tty device on line %d\n",
		       uport->line);
	}

	/*
	 * Ensure UPF_DEAD is not set.
	 */
	uport->flags &= ~UPF_DEAD;

 out:
	mutex_unlock(&port->mutex);
	mutex_unlock(&port_mutex);

	return ret;
}



其他  p {------------------------------------------------------------------------------- 
int tty_register_driver(struct tty_driver *driver)
{
	int error;
	int i;
	dev_t dev;
	struct device *d;

	if (!driver->major) {
		error = alloc_chrdev_region(&dev, driver->minor_start,
						driver->num, driver->name);
		if (!error) {
			driver->major = MAJOR(dev);
			driver->minor_start = MINOR(dev);
		}
	} else {
		dev = MKDEV(driver->major, driver->minor_start);
		error = register_chrdev_region(dev, driver->num, driver->name); // 申请 driver->num 个设备号
	}
	if (error < 0)
		goto err;

	if (driver->flags & TTY_DRIVER_DYNAMIC_ALLOC) { // 如果 TTY_DRIVER_DYNAMIC_ALLOC 标志被设置
		error = tty_cdev_add(struct tty_driver *driver, dev_t dev, unsigned int index = 0, unsigned int count, driver->num)
			cdev_init(&driver->cdevs[index], &tty_fops);
			driver->cdevs[index].owner = driver->owner;
			return cdev_add(&driver->cdevs[index], dev, count);  // 创建 driver->num 个字符设备，操作函数集是 tty_fops, 其字符设备保存在 driver->cdevs[] 中
		if (error)
			goto err_unreg_char;
	}

	mutex_lock(&tty_mutex);
	list_add(&driver->tty_drivers, &tty_drivers); // 将 tty_driver 挂到全局链表 tty_drivers 下
	mutex_unlock(&tty_mutex);

	if (!(driver->flags & TTY_DRIVER_DYNAMIC_DEV)) { // 如果 TTY_DRIVER_DYNAMIC_DEV 标志没有被设置
		for (i = 0; i < driver->num; i++) { // 创建 driver->num 个设备，包括 sysfs 下的设备和 /dev 下的设备节点
			d = tty_register_device(driver, i, NULL);
				return struct device *tty_register_device_attr(struct tty_driver *driver, unsigned index = i, struct device *device = NULL, void *drvdata = NULL, const struct attribute_group **attr_grp = NULL)
				{
					char name[64];
					dev_t devt = MKDEV(driver->major, driver->minor_start) + index;
					struct device *dev = NULL;
					int retval = -ENODEV;
					bool cdev = false;

					if (index >= driver->num) {
						printk(KERN_ERR "Attempt to register invalid tty line number (%d).\n", index);
						return ERR_PTR(-EINVAL);
					}

					if (driver->type == TTY_DRIVER_TYPE_PTY)
						pty_line_name(driver, index, name);
					else
						tty_line_name(driver, index, name);

					if (!(driver->flags & TTY_DRIVER_DYNAMIC_ALLOC)) { // 如果 TTY_DRIVER_DYNAMIC_ALLOC 标志没有被设置
						retval = tty_cdev_add(driver, devt, index, 1);
						if (retval)
							goto error;
						cdev = true;
					}

					dev = kzalloc(sizeof(*dev), GFP_KERNEL);
					if (!dev) {
						retval = -ENOMEM;
						goto error;
					}

					dev->devt = devt;
					dev->class = tty_class;
					dev->parent = device;
					dev->release = tty_device_create_release;
					dev_set_name(dev, "%s", name);
					dev->groups = attr_grp;
					dev_set_drvdata(dev, drvdata);

					retval = device_register(dev); // 创建设备和设备节点
					if (retval)
						goto error;

					return dev;

				error:
					put_device(dev);
					if (cdev)
						cdev_del(&driver->cdevs[index]);
					return ERR_PTR(retval);
				}
			if (IS_ERR(d)) {
				error = PTR_ERR(d);
				goto err_unreg_devs;
			}
		}
	}
	proc_tty_register_driver(driver); // 在 /proc 下创建驱动相关的文件
	driver->flags |= TTY_DRIVER_INSTALLED;
	return 0;

err_unreg_devs:
	for (i--; i >= 0; i--)
		tty_unregister_device(driver, i);

	mutex_lock(&tty_mutex);
	list_del(&driver->tty_drivers);
	mutex_unlock(&tty_mutex);

err_unreg_char:
	unregister_chrdev_region(dev, driver->num);
err:
	return error;
}

}{-------------------------------------------------------------------------------
