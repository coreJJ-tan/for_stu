1 前言
	printk() 函数最终会调用到 vprintk_emit() 实现, 这个函数, 查看内核发现, 调用 vprintk_emit() 函数的地方也不少, 也就是说, 调用
这个函数的其他函数都能实现打印, 这些函数包括:
	asmlinkage int vprintk(const char *fmt, va_list args)
	asmlinkage int printk_emit(int facility, int level, const char *dict, size_t dictlen, const char *fmt, ...)
	int vprintk_default(const char *fmt, va_list args) // 该函数又被 printk() 调用
	int printk_deferred(const char *fmt, ...)

2 printk 执行流程
	printk -> vprintk_emit -> console_unlock -> call_console_drivers
/**
 * printk - print a kernel message
 * @fmt: format string
 *
 * This is printk(). It can be called from any context. We want it to work.
 *
 * We try to grab the console_lock. If we succeed, it's easy - we log the
 * output and call the console drivers.  If we fail to get the semaphore, we
 * place the output into the log buffer and return. The current holder of
 * the console_sem will notice the new output in console_unlock(); and will
 * send it to the consoles before releasing the lock.
 *
 * One effect of this deferred printing is that code which calls printk() and
 * then changes console_loglevel may break. This is because console_loglevel
 * is inspected when the actual printing occurs.
 *
 * See also:
 * printf(3)
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
asmlinkage __visible int printk(const char *fmt, ...)
{
	printk_func_t vprintk_func; // 定义了一个函数指针
	va_list args;
	int r;

	va_start(args, fmt);

	/*
	 * If a caller overrides the per_cpu printk_func, then it needs
	 * to disable preemption when calling printk(). Otherwise
	 * the printk_func should be set to the default. No need to
	 * disable preemption here.
	 */
	vprintk_func = this_cpu_read(printk_func);
	r = vprintk_func(fmt, args); // 执行 vprintk_default 函数。

	va_end(args);

	return r;
}

typedef int(*printk_func_t)(const char *fmt, va_list args);

/*
 * This allows printk to be diverted to another function per cpu.
 * This is useful for calling printk functions from within NMI
 * without worrying about race conditions that can lock up the
 * box.
 */
DEFINE_PER_CPU(printk_func_t, printk_func) = vprintk_default; // 在 .data.percpu 段中定义了一个函数指针 per_cpu__printk_func， 类型是 printk_func_t
															  // 这个函数指针指向 vprintk_default 函数。
int vprintk_default(const char *fmt, va_list args)
{
	int r;
#ifdef CONFIG_KGDB_KDB // 用 kGDB 调试 Linux 内核时需要打开这个宏，一般不打开，
	if (unlikely(kdb_trap_printk)) {
		r = vkdb_printf(KDB_MSGSRC_PRINTK, fmt, args); // 先不考虑该情况
		return r;
	}
#endif
	r = vprintk_emit(0, LOGLEVEL_DEFAULT, NULL, 0, fmt, args);

	return r;
}

asmlinkage int vprintk_emit(int facility, int level, const char *dict, size_t dictlen, const char *fmt, va_list args)
{
	static int recursion_bug;
	static char textbuf[LOG_LINE_MAX];
	char *text = textbuf;
	size_t text_len = 0;
	enum log_flags lflags = 0;
	unsigned long flags;
	int this_cpu;
	int printed_len = 0;
	bool in_sched = false;
	/* cpu currently holding logbuf_lock in this function */
	static unsigned int logbuf_cpu = UINT_MAX;

	if (level == LOGLEVEL_SCHED) {
		level = LOGLEVEL_DEFAULT;
		in_sched = true;
	}

	boot_delay_msec(level);
	printk_delay();

	/* This stops the holder of console_sem just where we want him */
	local_irq_save(flags);
	this_cpu = smp_processor_id();

	/*
	 * Ouch, printk recursed into itself!
	 */
	if (unlikely(logbuf_cpu == this_cpu)) {
		/*
		 * If a crash is occurring during printk() on this CPU,
		 * then try to get the crash message out but make sure
		 * we can't deadlock. Otherwise just return to avoid the
		 * recursion and return - but flag the recursion so that
		 * it can be printed at the next appropriate moment:
		 */
		if (!oops_in_progress && !lockdep_recursing(current)) {
			recursion_bug = 1;
			local_irq_restore(flags);
			return 0;
		}
		zap_locks();
	}

	lockdep_off();
	raw_spin_lock(&logbuf_lock);
	logbuf_cpu = this_cpu;

	if (unlikely(recursion_bug)) {
		static const char recursion_msg[] =
			"BUG: recent printk recursion!";

		recursion_bug = 0;
		/* emit KERN_CRIT message */
		printed_len += log_store(0, 2, LOG_PREFIX|LOG_NEWLINE, 0,
					 NULL, 0, recursion_msg,
					 strlen(recursion_msg));
	}

	/*
	 * The printf needs to come first; we need the syslog
	 * prefix which might be passed-in as a parameter.
	 */
	text_len = vscnprintf(text, sizeof(textbuf), fmt, args);

	/* mark and strip a trailing newline */
	if (text_len && text[text_len-1] == '\n') {
		text_len--;
		lflags |= LOG_NEWLINE;
	}

	/* strip kernel syslog prefix and extract log level or control flags */
	if (facility == 0) {
		int kern_level = printk_get_level(text);

		if (kern_level) {
			const char *end_of_header = printk_skip_level(text);
			switch (kern_level) {
			case '0' ... '7':
				if (level == LOGLEVEL_DEFAULT)
					level = kern_level - '0';
				/* fallthrough */
			case 'd':	/* KERN_DEFAULT */
				lflags |= LOG_PREFIX;
			}
			/*
			 * No need to check length here because vscnprintf
			 * put '\0' at the end of the string. Only valid and
			 * newly printed level is detected.
			 */
			text_len -= end_of_header - text;
			text = (char *)end_of_header;
		}
	}

	if (level == LOGLEVEL_DEFAULT)
		level = default_message_loglevel;

	if (dict)
		lflags |= LOG_PREFIX|LOG_NEWLINE;

	if (!(lflags & LOG_NEWLINE)) {
		/*
		 * Flush the conflicting buffer. An earlier newline was missing,
		 * or another task also prints continuation lines.
		 */
		if (cont.len && (lflags & LOG_PREFIX || cont.owner != current))
			cont_flush(LOG_NEWLINE);

		/* buffer line if possible, otherwise store it right away */
		if (cont_add(facility, level, text, text_len))
			printed_len += text_len;
		else
			printed_len += log_store(facility, level,
						 lflags | LOG_CONT, 0,
						 dict, dictlen, text, text_len);
	} else {
		bool stored = false;

		/*
		 * If an earlier newline was missing and it was the same task,
		 * either merge it with the current buffer and flush, or if
		 * there was a race with interrupts (prefix == true) then just
		 * flush it out and store this line separately.
		 * If the preceding printk was from a different task and missed
		 * a newline, flush and append the newline.
		 */
		if (cont.len) {
			if (cont.owner == current && !(lflags & LOG_PREFIX))
				stored = cont_add(facility, level, text,
						  text_len);
			cont_flush(LOG_NEWLINE);
		}

		if (stored)
			printed_len += text_len;
		else
			printed_len += log_store(facility, level, lflags, 0,
						 dict, dictlen, text, text_len);
	}

	logbuf_cpu = UINT_MAX;
	raw_spin_unlock(&logbuf_lock);
	lockdep_on();
	local_irq_restore(flags);

	/* If called from the scheduler, we can not call up(). */
	if (!in_sched) {
		lockdep_off();
		/*
		 * Disable preemption to avoid being preempted while holding
		 * console_sem which would prevent anyone from printing to
		 * console
		 */
		preempt_disable();

		/*
		 * Try to acquire and then immediately release the console
		 * semaphore.  The release will print out buffers and wake up
		 * /dev/kmsg and syslog() users.
		 */
		if (console_trylock_for_printk())
			console_unlock();
		preempt_enable();
		lockdep_on();
	}

	return printed_len;
}
/**
 * console_unlock - unlock the console system
 *
 * Releases the console_lock which the caller holds on the console system
 * and the console driver list.
 *
 * While the console_lock was held, console output may have been buffered
 * by printk().  If this is the case, console_unlock(); emits
 * the output prior to releasing the lock.
 *
 * If there is output waiting, we wake /dev/kmsg and syslog() users.
 *
 * console_unlock(); may be called from any context.
 */
void console_unlock(void)
{
	static char text[LOG_LINE_MAX + PREFIX_MAX];
	static u64 seen_seq;
	unsigned long flags;
	bool wake_klogd = false;
	bool retry;

	if (console_suspended) {
		up_console_sem();
		return;
	}

	console_may_schedule = 0;

	/* flush buffered message fragment immediately to console */
	console_cont_flush(text, sizeof(text));
again:
	for (;;) {
		struct printk_log *msg;
		size_t len;
		int level;

		raw_spin_lock_irqsave(&logbuf_lock, flags);
		if (seen_seq != log_next_seq) {
			wake_klogd = true;
			seen_seq = log_next_seq;
		}

		if (console_seq < log_first_seq) {
			len = sprintf(text, "** %u printk messages dropped ** ",
				      (unsigned)(log_first_seq - console_seq));

			/* messages are gone, move to first one */
			console_seq = log_first_seq;
			console_idx = log_first_idx;
			console_prev = 0;
		} else {
			len = 0;
		}
skip:
		if (console_seq == log_next_seq)
			break;

		msg = log_from_idx(console_idx);
		if (msg->flags & LOG_NOCONS) {
			/*
			 * Skip record we have buffered and already printed
			 * directly to the console when we received it.
			 */
			console_idx = log_next(console_idx);
			console_seq++;
			/*
			 * We will get here again when we register a new
			 * CON_PRINTBUFFER console. Clear the flag so we
			 * will properly dump everything later.
			 */
			msg->flags &= ~LOG_NOCONS;
			console_prev = msg->flags;
			goto skip;
		}

		level = msg->level;
		len += msg_print_text(msg, console_prev, false,
				      text + len, sizeof(text) - len);
		console_idx = log_next(console_idx);
		console_seq++;
		console_prev = msg->flags;
		raw_spin_unlock(&logbuf_lock);

		stop_critical_timings();	/* don't trace print latency */
		call_console_drivers(level, text, len);
		start_critical_timings();
		local_irq_restore(flags);
	}
	console_locked = 0;

	/* Release the exclusive_console once it is used */
	if (unlikely(exclusive_console))
		exclusive_console = NULL;

	raw_spin_unlock(&logbuf_lock);

	up_console_sem();

	/*
	 * Someone could have filled up the buffer again, so re-check if there's
	 * something to flush. In case we cannot trylock the console_sem again,
	 * there's a new owner and the console_unlock() from them will do the
	 * flush, no worries.
	 */
	raw_spin_lock(&logbuf_lock);
	retry = console_seq != log_next_seq;
	raw_spin_unlock_irqrestore(&logbuf_lock, flags);

	if (retry && console_trylock())
		goto again;

	if (wake_klogd)
		wake_up_klogd();
}
/*
 * Call the console drivers, asking them to write out
 * log_buf[start] to log_buf[end - 1].
 * The console_lock must be held.
 */
static void call_console_drivers(int level, const char *text, size_t len)
{
	struct console *con;

	trace_console(text, len);

	if (level >= console_loglevel && !ignore_loglevel)
		return;
	if (!console_drivers)
		return;

	for_each_console(con) {
		if (exclusive_console && con != exclusive_console)
			continue;
		if (!(con->flags & CON_ENABLED))
			continue;
		if (!con->write)
			continue;
		if (!cpu_online(smp_processor_id()) &&
		    !(con->flags & CON_ANYTIME))
			continue;
		con->write(con, text, len);
	}
}