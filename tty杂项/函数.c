/**
 *	tty_init_dev		-	initialise a tty device
 *	@driver: tty driver we are opening a device on
 *	@idx: device index
 *	@ret_tty: returned tty structure
 *
 *	Prepare a tty device. This may not be a "new" clean device but
 *	could also be an active device. The pty drivers require special
 *	handling because of this.
 *
 *	Locking:
 *		The function is called under the tty_mutex, which
 *	protects us from the tty struct or driver itself going away.
 *
 *	On exit the tty device has the line discipline attached and
 *	a reference count of 1. If a pair was created for pty/tty use
 *	and the other was a pty master then it too has a reference count of 1.
 *
 * WSH 06/09/97: Rewritten to remove races and properly clean up after a
 * failed open.  The new code protects the open with a mutex, so it's
 * really quite straightforward.  The mutex locking can probably be
 * relaxed for the (most common) case of reopening a tty.
 */

struct tty_struct *tty_init_dev(struct tty_driver *driver, int idx)
{
	struct tty_struct *tty;
	int retval;

	/*
	 * First time open is complex, especially for PTY devices.
	 * This code guarantees that either everything succeeds and the
	 * TTY is ready for operation, or else the table slots are vacated
	 * and the allocated memory released.  (Except that the termios
	 * and locked termios may be retained.)
	 */

	if (!try_module_get(driver->owner))
		return ERR_PTR(-ENODEV);

	tty = alloc_tty_struct(driver, idx); // 初始化 tty_struct 的线路规程， 具体是 tty_struct->ldisc 成员 
                                         // ......
	if (!tty) {
		retval = -ENOMEM;
		goto err_module_put;
	}

	tty_lock(tty);
	retval = tty_driver_install_tty(driver, tty); // 绑定 tty_struct 到 tty_driver, 同时初始化 tty_struct 的 ktermios.
	if (retval < 0)
		goto err_deinit_tty;

	if (!tty->port)
		tty->port = driver->ports[idx]; // 绑定一个 tty 端口到 tty_struct(driver->ports 是在申请 tty_driver 阶段跟着申请内存的, 这个数组
                                        // 和 tty_driver->ttys 类似, 不过其成员还未初始化)

	WARN_RATELIMIT(!tty->port,
			"%s: %s driver does not set tty->port. This will crash the kernel later. Fix the driver!\n",
			__func__, tty->driver->name);

	tty->port->itty = tty; // 绑定 tty_struct 到 tty_port

	/*
	 * Structures all installed ... call the ldisc open routines.
	 * If we fail here just call release_tty to clean up.  No need
	 * to decrement the use counts, as release_tty doesn't care.
	 */
    // 到此，相关的结构体已经初始化的差不多了
	retval = tty_ldisc_setup(tty, tty->link); // 打开线路规程
	if (retval)
		goto err_release_tty;
	/* Return the tty locked so that it cannot vanish under the caller */
	return tty;

err_deinit_tty:
	tty_unlock(tty);
	deinitialize_tty_struct(tty);
	free_tty_struct(tty);
err_module_put:
	module_put(driver->owner);
	return ERR_PTR(retval);

	/* call the tty release_tty routine to clean out this slot */
err_release_tty:
	tty_unlock(tty);
	printk_ratelimited(KERN_INFO "tty_init_dev: ldisc open failed, "
				 "clearing slot %d\n", idx);
	release_tty(tty, idx);
	return ERR_PTR(retval);
}

/**
 *	tty_driver_install_tty() - install a tty entry in the driver
 *	@driver: the driver for the tty
 *	@tty: the tty
 *
 *	Install a tty object into the driver tables. The tty->index field
 *	will be set by the time this is called. This method is responsible
 *	for ensuring any need additional structures are allocated and
 *	configured.
 *
 *	Locking: tty_mutex for now
 */
static int tty_driver_install_tty(struct tty_driver *driver, struct tty_struct *tty)
{ 绑定 tty_struct 到 tty_driver, 从而可以通过 tty_driver 获取到 tty_struct
  如果 driver->ops->install() 函数实现了, 那么使用这个函数来绑定, 如果没有实现, 那么绑定的方式是将 tty_struct 的指针赋值给 driver->ttys 数组成员,
索引为 tty_struct->index, 同时初始化 tty_struct 的 ktermios.
  
	return driver->ops->install ? driver->ops->install(driver, tty) : tty_standard_install(driver, tty);
}

int tty_standard_install(struct tty_driver *driver, struct tty_struct *tty)
{
	int ret = tty_init_termios(tty);
	if (ret)
		return ret;

	tty_driver_kref_get(driver);
	tty->count++;
	driver->ttys[tty->index] = tty;
	return 0;
}

/**
 *	tty_init_termios	-  helper for termios setup
 *	@tty: the tty to set up
 *
 *	Initialise the termios structures for this tty. Thus runs under
 *	the tty_mutex currently so we can be relaxed about ordering.
 */

int tty_init_termios(struct tty_struct *tty)
{ 设置 tty_struct 的 ktermios.
  (1) 如果 tty_driver 的 TTY_DRIVER_RESET_TERMIOS 标志被设置, 则 tty_struct 使用 tty_driver->init_termios 作为自己的 ktermios, 即使
用 tty_driver 事先设置的默认的 ktermios.
  (2) 如果没有被设置, 则从 tty_driver->termios[] 数组中根据 tty->index 获取 ktermios, 获取不到还是会设置成 tty_driver->init_termios
	struct ktermios *tp;
	int idx = tty->index;

	if (tty->driver->flags & TTY_DRIVER_RESET_TERMIOS)
		tty->termios = tty->driver->init_termios;
	else {
		/* Check for lazy saved data */
		tp = tty->driver->termios[idx];
		if (tp != NULL)
			tty->termios = *tp;
		else
			tty->termios = tty->driver->init_termios;
	}
	/* Compatibility until drivers always set this */
	tty->termios.c_ispeed = tty_termios_input_baud_rate(&tty->termios); // 将 ktermios 解析成实际的波特率数值
	tty->termios.c_ospeed = tty_termios_baud_rate(&tty->termios); // 将 ktermios 解析成实际的波特率数值
	return 0;
}

/**
 *	tty_ldisc_init		-	ldisc setup for new tty
 *	@tty: tty being allocated
 *
 *	Set up the line discipline objects for a newly allocated tty. Note that
 *	the tty structure is not completely set up when this call is made.
 */

void tty_ldisc_init(struct tty_struct *tty)
{
	struct tty_ldisc *ld = tty_ldisc_get(tty, N_TTY); // 获取线路规程以及操作函数集, N_TTY 对应的线路规程在 xxx 阶段就被初始化了
	if (IS_ERR(ld))
		panic("n_tty: init_tty");
	tty->ldisc = ld; // 绑定线路规程到 tty_struct
}
/**
 *	tty_ldisc_get		-	take a reference to an ldisc
 *	@disc: ldisc number
 *
 *	Takes a reference to a line discipline. Deals with refcounts and
 *	module locking counts. Returns NULL if the discipline is not available.
 *	Returns a pointer to the discipline and bumps the ref count if it is
 *	available
 *
 *	Locking:
 *		takes tty_ldiscs_lock to guard against ldisc races
 */

static struct tty_ldisc *tty_ldisc_get(struct tty_struct *tty, int disc)
{
	struct tty_ldisc *ld;
	struct tty_ldisc_ops *ldops;

	if (disc < N_TTY || disc >= NR_LDISCS)
		return ERR_PTR(-EINVAL);

	/*
	 * Get the ldisc ops - we may need to request them to be loaded
	 * dynamically and try again.
	 */
	ldops = get_ldops(disc); // 获取线路规程操作函数集
	if (IS_ERR(ldops)) {
		request_module("tty-ldisc-%d", disc);
		ldops = get_ldops(disc);
		if (IS_ERR(ldops))
			return ERR_CAST(ldops);
	}

	ld = kmalloc(sizeof(struct tty_ldisc), GFP_KERNEL); // 申请一个线路规程
	if (ld == NULL) {
		put_ldops(ldops);
		return ERR_PTR(-ENOMEM);
	}

	ld->ops = ldops; // 将函数集与线路规程绑定
	ld->tty = tty; // 绑定 tty_struct 到线路规程

	return ld;
}

static struct tty_ldisc_ops *get_ldops(int disc)
{
	unsigned long flags;
	struct tty_ldisc_ops *ldops, *ret;

	raw_spin_lock_irqsave(&tty_ldiscs_lock, flags);
	ret = ERR_PTR(-EINVAL);
	ldops = tty_ldiscs[disc]; // 从全局数组获取线路规程函数集
	if (ldops) {
		ret = ERR_PTR(-EAGAIN);
		if (try_module_get(ldops->owner)) {
			ldops->refcount++;
			ret = ldops;
		}
	}
	raw_spin_unlock_irqrestore(&tty_ldiscs_lock, flags);
	return ret;
}
/**
 *	tty_ldisc_setup			-	open line discipline
 *	@tty: tty being shut down
 *	@o_tty: pair tty for pty/tty pairs
 *
 *	Called during the initial open of a tty/pty pair in order to set up the
 *	line disciplines and bind them to the tty. This has no locking issues
 *	as the device isn't yet active.
 */

int tty_ldisc_setup(struct tty_struct *tty, struct tty_struct *o_tty)
{
	struct tty_ldisc *ld = tty->ldisc;
	int retval;

	retval = tty_ldisc_open(tty, ld);
	if (retval)
		return retval;

	if (o_tty) {
		retval = tty_ldisc_open(o_tty, o_tty->ldisc);
		if (retval) {
			tty_ldisc_close(tty, ld);
			return retval;
		}
	}
	return 0;
}

/**
 *	tty_ldisc_open		-	open a line discipline
 *	@tty: tty we are opening the ldisc on
 *	@ld: discipline to open
 *
 *	A helper opening method. Also a convenient debugging and check
 *	point.
 *
 *	Locking: always called with BTM already held.
 */

static int tty_ldisc_open(struct tty_struct *tty, struct tty_ldisc *ld)
{
	WARN_ON(test_and_set_bit(TTY_LDISC_OPEN, &tty->flags)); // 设置线路规程 TTY_LDISC_OPEN，表示线路规程已被打开
	if (ld->ops->open) { // 调用线路规程函数集 open() 函数来打开线路规程, 如果 open() 函数未实现,则表示不需要特意打开，默认已打开
		int ret;
                /* BTM here locks versus a hangup event */
		ret = ld->ops->open(tty);
		if (ret)
			clear_bit(TTY_LDISC_OPEN, &tty->flags);
		return ret;
	}
	return 0;
}

/**
 *	flush_to_ldisc
 *	@work: tty structure passed from work queue.
 *
 *	This routine is called out of the software interrupt to flush data from the buffer chain to the line discipline.
 *	The receive_buf method is single threaded for each tty instance.
 *
 *	Locking: takes buffer lock to ensure single-threaded flip buffer
 *		 'consumer'
 */

static void flush_to_ldisc(struct work_struct *work)
{
	struct tty_port *port = container_of(work, struct tty_port, buf.work);
	struct tty_bufhead *buf = &port->buf;
	struct tty_struct *tty;
	struct tty_ldisc *disc;

	tty = port->itty; // port->itty 是在哪里被初始化的？
	if (tty == NULL)
		return;

	disc = tty_ldisc_ref(tty);
	if (disc == NULL)
		return;

	mutex_lock(&buf->lock);

	while (1) {
		struct tty_buffer *head = buf->head;
		struct tty_buffer *next;
		int count;

		/* Ldisc or user is trying to gain exclusive access */
		if (atomic_read(&buf->priority))
			break;

		next = head->next;
		/* paired w/ barrier in __tty_buffer_request_room();
		 * ensures commit value read is not stale if the head
		 * is advancing to the next buffer
		 */
		smp_rmb();
		count = head->commit - head->read;
		if (!count) { // count = 0 表明第一个 head 缓冲区已经刷新完毕
			if (next == NULL) {
				check_other_closed(tty);
				break;
			}
			buf->head = next;	// 获取第二个 head 缓冲区
			tty_buffer_free(port, head);
			continue;	// 继续刷新第二个缓冲区的数据
		}

		count = receive_buf(tty, head, count);
		if (!count)
			break;
	}

	mutex_unlock(&buf->lock);

	tty_ldisc_deref(disc);
}
static int receive_buf(struct tty_struct *tty, struct tty_buffer *head, int count)
{
	struct tty_ldisc *disc = tty->ldisc;
	unsigned char *p = char_buf_ptr(head, head->read);
	char	      *f = NULL;

	if (~head->flags & TTYB_NORMAL) // tty_buffer 的 TTYB_NORMAL 标志没有被设置，说明缓冲区保存有字符的标志
		f = flag_buf_ptr(head, head->read);

	if (disc->ops->receive_buf2)
		count = disc->ops->receive_buf2(tty, p, f, count); // 优先使用 ->receive_buf2() 进行刷新， 在 N_TTY 线路规程中被指定为 n_tty_receive_buf2() 函数
	else {
		count = min_t(int, count, tty->receive_room);
		if (count)
			disc->ops->receive_buf(tty, p, f, count); //  在 N_TTY 线路规程中被指定为 n_tty_receive_buf() 函数
	}
	head->read += count;
	return count; // 返回本次刷新的字节数
}
static void n_tty_receive_buf(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	n_tty_receive_buf_common(tty, cp, fp, count, 0);
}
static int n_tty_receive_buf2(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	return n_tty_receive_buf_common(tty, cp, fp, count, 1);
}
/**
 *	n_tty_receive_buf_common	-	process input
 *	@tty: device to receive input
 *	@cp: input chars
 *	@fp: flags for each char (if NULL, all chars are TTY_NORMAL)
 *	@count: number of input chars in @cp
 *
 *	Called by the terminal driver when a block of characters has
 *	been received. This function must be called from soft contexts
 *	not from interrupt context. The driver is responsible for making
 *	calls one at a time and in order (or using flush_to_ldisc)
 *
 *	Returns the # of input chars from @cp which were processed.
 *
 *	In canonical mode, the maximum line length is 4096 chars (including
 *	the line termination char); lines longer than 4096 chars are
 *	truncated. After 4095 chars, input data is still processed but
 *	not stored. Overflow processing ensures the tty can always
 *	receive more input until at least one line can be read.
 *
 *	In non-canonical mode, the read buffer will only accept 4095 chars;
 *	this provides the necessary space for a newline char if the input
 *	mode is switched to canonical.
 *
 *	Note it is possible for the read buffer to _contain_ 4096 chars
 *	in non-canonical mode: the read buffer could already contain the
 *	maximum canon line of 4096 chars when the mode is switched to
 *	non-canonical.
 *
 *	n_tty_receive_buf()/producer path:
 *		claims non-exclusive termios_rwsem
 *		publishes commit_head or canon_head
 */
static int n_tty_receive_buf_common(struct tty_struct *tty, const unsigned char *cp, char *fp, int count, int flow)
{
	struct n_tty_data *ldata = tty->disc_data;
	int room, n, rcvd = 0, overflow;

	down_read(&tty->termios_rwsem);

	while (1) {
		/*
		 * When PARMRK is set, each input char may take up to 3 chars
		 * in the read buf; reduce the buffer space avail by 3x
		 *
		 * If we are doing input canonicalization, and there are no
		 * pending newlines, let characters through without limit, so
		 * that erase characters will be handled.  Other excess
		 * characters will be beeped.
		 *
		 * paired with store in *_copy_from_read_buf() -- guarantees
		 * the consumer has loaded the data in read_buf up to the new
		 * read_tail (so this producer will not overwrite unread data)
		 */
		size_t tail = smp_load_acquire(&ldata->read_tail);

		room = N_TTY_BUF_SIZE - (ldata->read_head - tail);
		if (I_PARMRK(tty))
			room = (room + 2) / 3;
		room--;
		if (room <= 0) {
			overflow = ldata->icanon && ldata->canon_head == tail;
			if (overflow && room < 0)
				ldata->read_head--;
			room = overflow;
			ldata->no_room = flow && !room;
		} else
			overflow = 0;

		n = min(count, room);
		if (!n)
			break;

		/* ignore parity errors if handling overflow */
		if (!overflow || !fp || *fp != TTY_PARITY)
			__receive_buf(tty, cp, fp, n);

		cp += n;
		if (fp)
			fp += n;
		count -= n;
		rcvd += n;
	}

	tty->receive_room = room;

	/* Unthrottle if handling overflow on pty */
	if (tty->driver->type == TTY_DRIVER_TYPE_PTY) {
		if (overflow) {
			tty_set_flow_change(tty, TTY_UNTHROTTLE_SAFE);
			tty_unthrottle_safe(tty);
			__tty_set_flow_change(tty, 0);
		}
	} else
		n_tty_check_throttle(tty);

	up_read(&tty->termios_rwsem);

	return rcvd;
}