uart_add_one_port
    tty_port_register_device_attr       // tty_dev = tty_port_register_device_attr(port, drv->tty_driver, uport->line, uport->dev, port, uport->tty_groups);
        tty_register_device_attr  // tty_register_device_attr(driver, index, device, drvdata, attr_grp);
            tty_cdev_add    // if (!(driver->flags & TTY_DRIVER_DYNAMIC_ALLOC))
            device_register

uart_register_driver
    tty_register_driver
        tty_cdev_add    // if (driver->flags & TTY_DRIVER_DYNAMIC_ALLOC)
        tty_register_device     // if (!(driver->flags & TTY_DRIVER_DYNAMIC_DEV))
            tty_register_device_attr
                tty_cdev_add    // if (!(driver->flags & TTY_DRIVER_DYNAMIC_ALLOC))
                device_register
    

uart_add_one_port
    tty_dev = struct device *tty_port_register_device_attr(struct tty_port *port = port, struct tty_driver *driver = drv->tty_driver, unsigned index = uport->line,
		                    struct device *device = uport->dev, void *drvdata = port, const struct attribute_group **attr_grp = uport->tty_groups)
        tty_port_link_device(port, driver, index);
        return tty_register_device_attr(driver, index, device, drvdata, attr_grp);

uart_register_driver
    tty_register_driver
        if (driver->flags & TTY_DRIVER_DYNAMIC_ALLOC)
		error = tty_cdev_add(driver, dev, 0, driver->num);
		if (error)
			goto err_unreg_char;
        if (!(driver->flags & TTY_DRIVER_DYNAMIC_DEV))
            for (i = 0; i < driver->num; i++)
                d = tty_register_device(driver, i, NULL);
                    return tty_register_device_attr(driver, index, device, NULL, NULL);
                if (IS_ERR(d))
                    error = PTR_ERR(d);
                    goto err_unreg_devs;



struct device *tty_register_device_attr(struct tty_driver *driver, unsigned index, struct device *device, void *drvdata, const struct attribute_group **attr_grp)
{
	char name[64];
	dev_t devt = MKDEV(driver->major, driver->minor_start) + index;
	struct device *dev = NULL;
	int retval = -ENODEV;
	bool cdev = false;

	if (index >= driver->num) {
		printk(KERN_ERR "Attempt to register invalid tty line number "
		       " (%d).\n", index);
		return ERR_PTR(-EINVAL);
	}

	if (driver->type == TTY_DRIVER_TYPE_PTY)
		pty_line_name(driver, index, name);
	else
		tty_line_name(driver, index, name);

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

	retval = device_register(dev);
	if (retval)
		goto error;

	return dev;

error:
	put_device(dev);
	if (cdev)
		cdev_del(&driver->cdevs[index]);
	return ERR_PTR(retval);
}