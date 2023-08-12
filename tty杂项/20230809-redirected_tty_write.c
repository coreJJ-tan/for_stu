ssize_t redirected_tty_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct file *p = NULL;

	spin_lock(&redirect_lock);
	if (redirect) // redirect 是个静态局部变量, static struct file *redirect; 用于控制台重定向
		p = get_file(redirect);
	spin_unlock(&redirect_lock);

	if (p) {
		ssize_t res;
		res = vfs_write(p, buf, count, &p->f_pos);
		fput(p);
		return res;
	}
	return tty_write(file, buf, count, ppos);
}
static inline struct file *get_file(struct file *f)
{
	atomic_long_inc(&f->f_count);
	return f;
}