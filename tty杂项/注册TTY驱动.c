int tty_register_driver(struct tty_driver *driver)
{ 注册一个 tty_driver.
  (1) tty_driver 是驱动 tty 设备的, 因此需要给 tty 设备申请 tty_driver->num 数量的设备号, 设备号可以自己定义, 也可以随机分配.
  (2) 如果 tty_driver 的 TTY_DRIVER_DYNAMIC_ALLOC 标志事先被设置, 那么将创建 tty_driver->num 数量的 tty 字符设备, 操作函数集是 tty_fops(/* 这些字符设备的名字？ */).
  (3) 该 tty_driver 通过 tty_driver->tty_drivers 挂在 tty_drivers 全局链表下(/* 访问该链表，需要使用 tty_mutex 锁 */).
  (4) 如果 tty_driver 的 TTY_DRIVER_DYNAMIC_DEV 标志事先不被设置, 
  (5)
  (6) 设置 tty_driver 的 TTY_DRIVER_INSTALLED 标志, 表明该 tty 驱动已经被注册成功.
	int error;
	int i;
	dev_t dev;
	struct device *d;

    // 申请设备号, 设备号可以事先提供, 也可以由内核自动分配
	if (!driver->major) {
		error = alloc_chrdev_region(&dev, driver->minor_start, driver->num, driver->name);
		if (!error) {
			driver->major = MAJOR(dev);
			driver->minor_start = MINOR(dev);
		}
	} else {
		dev = MKDEV(driver->major, driver->minor_start);
		error = register_chrdev_region(dev, driver->num, driver->name);
	}
	if (error < 0)
		goto err;

	if (driver->flags & TTY_DRIVER_DYNAMIC_ALLOC) { // 由 uart_register_driver 注册的串口 tty 驱动并不会设置该标志
		error = tty_cdev_add(driver, dev, 0, driver->num); // 创建 tty 字符设备
		if (error)
			goto err_unreg_char;
	}

	mutex_lock(&tty_mutex);
	list_add(&driver->tty_drivers, &tty_drivers); // 将 tty 驱动加到 tty_drivers 链表中
	mutex_unlock(&tty_mutex);

	if (!(driver->flags & TTY_DRIVER_DYNAMIC_DEV)) { // 由 uart_register_driver 注册的串口 tty 驱动会同步设置该标志
		for (i = 0; i < driver->num; i++) { // 注册 driver->num 数量的 individual tty devices
			d = tty_register_device(driver, i, NULL);
			if (IS_ERR(d)) {
				error = PTR_ERR(d);
				goto err_unreg_devs;
			}
		}
	}
	proc_tty_register_driver(driver);
	driver->flags |= TTY_DRIVER_INSTALLED; // 表明该 tty 驱动已经被注册成功
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
static int tty_cdev_add(struct tty_driver *driver, dev_t dev, unsigned int index, unsigned int count)
{ 创建 count 数量的 tty字符设备.
	/* init here, since reused cdevs cause crashes */
	cdev_init(&driver->cdevs[index], &tty_fops);
	driver->cdevs[index].owner = driver->owner;
	return cdev_add(&driver->cdevs[index], dev, count);
}
struct device *tty_register_device(struct tty_driver *driver, unsigned index, struct device *device)
{
	return tty_register_device_attr(driver, index, device, NULL, NULL);
}
/**
 *	tty_register_device_attr - register a tty device
 *	@driver: the tty driver that describes the tty device
 *	@index: the index in the tty driver for this tty device
 *	@device: a struct device that is associated with this tty device.
 *		This field is optional, if there is no known struct device
 *		for this tty device it can be set to NULL safely.
 *	@drvdata: Driver data to be set to device.
 *	@attr_grp: Attribute group to be set on device.
 *
 *	Returns a pointer to the struct device for this tty device
 *	(or ERR_PTR(-EFOO) on error).
 *
 *	This call is required to be made to register an individual tty device
 *	if the tty driver's flags have the TTY_DRIVER_DYNAMIC_DEV bit set.  If
 *	that bit is not set, this function should not be called by a tty
 *	driver.
 *
 *	Locking: ??
 */
struct device *tty_register_device_attr(struct tty_driver *driver, unsigned index, struct device *device,
				   void *drvdata, const struct attribute_group **attr_grp)
{
	char name[64];
	dev_t devt = MKDEV(driver->major, driver->minor_start) + index; // 合成设备号
	struct device *dev = NULL;
	int retval = -ENODEV;
	bool cdev = false;

	if (index >= driver->num) {
		printk(KERN_ERR "Attempt to register invalid tty line number (%d).\n", index);
		return ERR_PTR(-EINVAL);
	}

	if (driver->type == TTY_DRIVER_TYPE_PTY) // 创建这个设备的名字，这个名字合成很有讲究，如果要研究 /dev 下的 tty 设备名字, 可以看这里
		pty_line_name(driver, index, name); // 该驱动是 TTY_DRIVER_TYPE_PTY 类型的
	else
		tty_line_name(driver, index, name); // 该驱动不是 TTY_DRIVER_TYPE_PTY 类型的， 比如会生成 /dev/ttyMSM 或者 /dev/ttyMSM0 这样的名字

	if (!(driver->flags & TTY_DRIVER_DYNAMIC_ALLOC)) {
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

	retval = device_register(dev); // 创建设备，会同时创建 sysfs 下的设备和 /dev 下的设备节点
	if (retval)
		goto error;

	return dev;

error:
	put_device(dev);
	if (cdev)
		cdev_del(&driver->cdevs[index]);
	return ERR_PTR(retval);
}
static char ptychar[] = "pqrstuvwxyzabcde";
/**
 *	pty_line_name	-	generate name for a pty
 *	@driver: the tty driver in use
 *	@index: the minor number
 *	@p: output buffer of at least 6 bytes
 *
 *	Generate a name from a driver reference and write it to the output
 *	buffer.
 *
 *	Locking: None
 */
static void pty_line_name(struct tty_driver *driver, int index, char *p)
{
	int i = index + driver->name_base;
	/* ->name is initialized to "ttyp", but "tty" is expected */
	sprintf(p, "%s%c%x", driver->subtype == PTY_TYPE_SLAVE ? "tty" : driver->name, ptychar[i >> 4 & 0xf], i & 0xf);
}
/**
 *	tty_line_name	-	generate name for a tty
 *	@driver: the tty driver in use
 *	@index: the minor number
 *	@p: output buffer of at least 7 bytes
 *
 *	Generate a name from a driver reference and write it to the output
 *	buffer.
 *
 *	Locking: None
 */
static ssize_t tty_line_name(struct tty_driver *driver, int index, char *p)
{
	if (driver->flags & TTY_DRIVER_UNNUMBERED_NODE)
		return sprintf(p, "%s", driver->name);
	else
		return sprintf(p, "%s%d", driver->name, index + driver->name_base);
}