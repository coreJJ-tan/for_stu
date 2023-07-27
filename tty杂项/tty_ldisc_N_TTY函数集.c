struct tty_ldisc_ops tty_ldisc_N_TTY = {
	.magic           = TTY_LDISC_MAGIC,
	.name            = "n_tty",
	.open            = n_tty_open,
	.close           = n_tty_close,
	.flush_buffer    = n_tty_flush_buffer,
	.chars_in_buffer = n_tty_chars_in_buffer,
	.read            = n_tty_read,
	.write           = n_tty_write,
	.ioctl           = n_tty_ioctl,
	.set_termios     = n_tty_set_termios,
	.poll            = n_tty_poll,
	.receive_buf     = n_tty_receive_buf,
	.write_wakeup    = n_tty_write_wakeup,
	.fasync		 = n_tty_fasync,
	.receive_buf2	 = n_tty_receive_buf2,
};

1、 回调函数 o{----------------------------------------------------------------------------------------------------------------
/**
 *	n_tty_open		-	open an ldisc
 *	@tty: terminal to open
 *
 *	Called when this line discipline is being attached to the terminal device. Can sleep. Called serialized so that no
 *	other events will occur in parallel. No further open will occur until a close.
 */

static int n_tty_open(struct tty_struct *tty)
{
	struct n_tty_data *ldata;

	/* Currently a malloc failure here can panic */
	ldata = vmalloc(sizeof(*ldata)); // 申请一个 n_tty_data
	if (!ldata)
		goto err;

	ldata->overrun_time = jiffies; /* 将当前时间拍数赋给 ldata->overrun_time */
	mutex_init(&ldata->atomic_read_lock);
	mutex_init(&ldata->output_lock);

	tty->disc_data = ldata; // 将上面申请的 n_tty_data 保存到 tty_struct 中，方便后续使用                
	reset_buffer_flags(struct n_tty_data *ldata = tty->disc_data); {// 初始化 n_tty_data
		ldata->read_head = ldata->canon_head = ldata->read_tail = 0;
		ldata->echo_head = ldata->echo_tail = ldata->echo_commit = 0;
		ldata->commit_head = 0;
		ldata->echo_mark = 0;
		ldata->line_start = 0;

		ldata->erasing = 0;
		bitmap_zero(ldata->read_flags, N_TTY_BUF_SIZE);
		ldata->push = 0;
	}
	ldata->column = 0;
	ldata->canon_column = 0;
	ldata->minimum_to_wake = 1;
	ldata->num_overrun = 0;
	ldata->no_room = 0;
	ldata->lnext = 0;
	tty->closing = 0;
	/* indicate buffer work may resume */
	clear_bit(TTY_LDISC_HALTED, &tty->flags); // 清除 TTY_LDISC_HALTED，表示线路规程从暂停/停止状态恢复
	n_tty_set_termios(tty, NULL); // 待定
	tty_unthrottle(tty); // TTY_THROTTLED标志被设置，则执行 tty_struct 函数集的 unthrottle() 函数，并将 
						 // flow_change 设置为 0.

	return 0;
err:
	return -ENOMEM;
}
}
------------------------------------------------------------------------------------------------------------------------------
1、 其它函数 o{----------------------------------------------------------------------------------------------------------------
/**
 *  tty_unthrottle      -   flow control
 *  @tty: terminal
 *
 *  Indicate that a tty may continue transmitting data down the stack.
 *  Takes the termios rwsem to protect against parallel throttle/unthrottle
 *  and also to ensure the driver can consistently reference its own
 *  termios data at this point when implementing software flow control.
 *
 *  Drivers should however remember that the stack can issue a throttle,
 *  then change flow control method, then unthrottle.
 */

void tty_unthrottle(struct tty_struct *tty)
{
    down_write(&tty->termios_rwsem);
    if (test_and_clear_bit(TTY_THROTTLED, &tty->flags) && tty->ops->unthrottle) // 检查 tty_struct 的 TTY_THROTTLED 标志是否被
																				// 设置，使得话则执行它的函数集中 unthrottle() 函数
        tty->ops->unthrottle(tty);
    tty->flow_change = 0;	// tty_struct 的 flow_change 设置为 0
    up_write(&tty->termios_rwsem);
}

/**
 *	n_tty_set_termios	-	termios data changed
 *	@tty: terminal
 *	@old: previous data
 *
 *	Called by the tty layer when the user changes termios flags so
 *	that the line discipline can plan ahead. This function cannot sleep
 *	and is protected from re-entry by the tty layer. The user is
 *	guaranteed that this function will not be re-entered or in progress
 *	when the ldisc is closed.
 *
 *	Locking: Caller holds tty->termios_rwsem
 */

static void n_tty_set_termios(struct tty_struct *tty, struct ktermios *old)
{
	struct n_tty_data *ldata = tty->disc_data;

	if (!old || (old->c_lflag ^ tty->termios.c_lflag) & ICANON) {
		bitmap_zero(ldata->read_flags, N_TTY_BUF_SIZE);
		ldata->line_start = ldata->read_tail;
		if (!L_ICANON(tty) || !read_cnt(ldata)) {
			ldata->canon_head = ldata->read_tail;
			ldata->push = 0;
		} else {
			set_bit((ldata->read_head - 1) & (N_TTY_BUF_SIZE - 1),
				ldata->read_flags);
			ldata->canon_head = ldata->read_head;
			ldata->push = 1;
		}
		ldata->commit_head = ldata->read_head;
		ldata->erasing = 0;
		ldata->lnext = 0;
	}

	ldata->icanon = (L_ICANON(tty) != 0);

	if (I_ISTRIP(tty) || I_IUCLC(tty) || I_IGNCR(tty) ||
	    I_ICRNL(tty) || I_INLCR(tty) || L_ICANON(tty) ||
	    I_IXON(tty) || L_ISIG(tty) || L_ECHO(tty) ||
	    I_PARMRK(tty)) {
		bitmap_zero(ldata->char_map, 256);

		if (I_IGNCR(tty) || I_ICRNL(tty))
			set_bit('\r', ldata->char_map);
		if (I_INLCR(tty))
			set_bit('\n', ldata->char_map);

		if (L_ICANON(tty)) {
			set_bit(ERASE_CHAR(tty), ldata->char_map);
			set_bit(KILL_CHAR(tty), ldata->char_map);
			set_bit(EOF_CHAR(tty), ldata->char_map);
			set_bit('\n', ldata->char_map);
			set_bit(EOL_CHAR(tty), ldata->char_map);
			if (L_IEXTEN(tty)) {
				set_bit(WERASE_CHAR(tty), ldata->char_map);
				set_bit(LNEXT_CHAR(tty), ldata->char_map);
				set_bit(EOL2_CHAR(tty), ldata->char_map);
				if (L_ECHO(tty))
					set_bit(REPRINT_CHAR(tty),
						ldata->char_map);
			}
		}
		if (I_IXON(tty)) {
			set_bit(START_CHAR(tty), ldata->char_map);
			set_bit(STOP_CHAR(tty), ldata->char_map);
		}
		if (L_ISIG(tty)) {
			set_bit(INTR_CHAR(tty), ldata->char_map);
			set_bit(QUIT_CHAR(tty), ldata->char_map);
			set_bit(SUSP_CHAR(tty), ldata->char_map);
		}
		clear_bit(__DISABLED_CHAR, ldata->char_map);
		ldata->raw = 0;
		ldata->real_raw = 0;
	} else {
		ldata->raw = 1;
		if ((I_IGNBRK(tty) || (!I_BRKINT(tty) && !I_PARMRK(tty))) &&
		    (I_IGNPAR(tty) || !I_INPCK(tty)) &&
		    (tty->driver->flags & TTY_DRIVER_REAL_RAW))
			ldata->real_raw = 1;
		else
			ldata->real_raw = 0;
	}
	/*
	 * Fix tty hang when I_IXON(tty) is cleared, but the tty
	 * been stopped by STOP_CHAR(tty) before it.
	 */
	if (!I_IXON(tty) && old && (old->c_iflag & IXON) && !tty->flow_stopped) {
		start_tty(tty);
		process_echoes(tty);
	}

	/* The termios change make the tty ready for I/O */
	wake_up_interruptible(&tty->write_wait);
	wake_up_interruptible(&tty->read_wait);
}
------------------------------------------------------------------------------------------------------------------------------
