struct n_tty_data {
	/* producer-published */
	size_t read_head;
	size_t commit_head;
	size_t canon_head;
	size_t echo_head;
	size_t echo_commit;
	size_t echo_mark;
	DECLARE_BITMAP(char_map, 256);

	/* private to n_tty_receive_overrun (single-threaded) */
	unsigned long overrun_time;
	int num_overrun;

	/* non-atomic */
	bool no_room;

	/* must hold exclusive termios_rwsem to reset these */
	unsigned char lnext:1, erasing:1, raw:1, real_raw:1, icanon:1;
	unsigned char push:1;

	/* shared by producer and consumer */
	char read_buf[N_TTY_BUF_SIZE];
	DECLARE_BITMAP(read_flags, N_TTY_BUF_SIZE);
	unsigned char echo_buf[N_TTY_BUF_SIZE];

	int minimum_to_wake;

	/* consumer-published */
	size_t read_tail;
	size_t line_start;

	/* protected by output lock */
	unsigned int column;
	unsigned int canon_column;
	size_t echo_tail;

	struct mutex atomic_read_lock;
	struct mutex output_lock;
};

struct tty_struct {
	int	magic;
	struct kref kref;
	struct device *dev;
	struct tty_driver *driver;
	const struct tty_operations *ops;
	int index;

	/* Protects ldisc changes: Lock tty not pty */
	struct ld_semaphore ldisc_sem;
	struct tty_ldisc *ldisc;

	struct mutex atomic_write_lock;
	struct mutex legacy_mutex;
	struct mutex throttle_mutex;
	struct rw_semaphore termios_rwsem;
	struct mutex winsize_mutex;
	spinlock_t ctrl_lock;
	spinlock_t flow_lock;
	/* Termios values are protected by the termios rwsem */
	struct ktermios termios, termios_locked;
	struct termiox *termiox;	/* May be NULL for unsupported */
	char name[64];
	struct pid *pgrp;		/* Protected by ctrl lock */
	struct pid *session;
	unsigned long flags;
	int count;
	struct winsize winsize;		/* winsize_mutex */
	unsigned long stopped:1,	/* flow_lock */
		      flow_stopped:1,
		      unused:BITS_PER_LONG - 2;
	int hw_stopped;
	unsigned long ctrl_status:8,	/* ctrl_lock */
		      packet:1,
		      unused_ctrl:BITS_PER_LONG - 9;
	unsigned int receive_room;	/* Bytes free for queue */
	int flow_change;

	struct tty_struct *link;
	struct fasync_struct *fasync;
	int alt_speed;		/* For magic substitution of 38400 bps */
	wait_queue_head_t write_wait;
	wait_queue_head_t read_wait;
	struct work_struct hangup_work;
	void *disc_data;
	void *driver_data;
	struct list_head tty_files;

#define N_TTY_BUF_SIZE 4096

	int closing;
	unsigned char *write_buf;
	int write_cnt;
	/* If the tty has a pending do_SAK, queue it here - akpm */
	struct work_struct SAK_work;
	struct tty_port *port;
};


struct tty_driver {
	int	magic;		/* magic number for this structure */
	struct kref kref;	/* Reference management */
	struct cdev *cdevs;
	struct module	*owner;
	const char	*driver_name;
	const char	*name;
	int	name_base;	/* offset of printed name */
	int	major;		/* major device number */
	int	minor_start;	/* start of minor device number */
	unsigned int	num;	/* number of devices allocated */
	short	type;		/* type of tty driver */
	short	subtype;	/* subtype of tty driver */
	struct ktermios init_termios; /* Initial termios */
	unsigned long	flags;		/* tty driver flags */
	struct proc_dir_entry *proc_entry; /* /proc fs entry */
	struct tty_driver *other; /* only used for the PTY driver */

	/*
	 * Pointer to the tty data structures
	 */
	struct tty_struct **ttys;
	struct tty_port **ports;
	struct ktermios **termios;
	void *driver_state;

	/*
	 * Driver methods
	 */

	const struct tty_operations *ops;
	struct list_head tty_drivers;
};
struct tty_port {
    struct tty_bufhead  buf;        /* Locked internally */
    struct tty_struct   *tty;       /* Back pointer */
    struct tty_struct   *itty;      /* internal back ptr */
    const struct tty_port_operations *ops;  /* Port operations */
    spinlock_t      lock;       /* Lock protecting tty field */
    int         blocked_open;   /* Waiting to open */
    int         count;      /* Usage count */
    wait_queue_head_t   open_wait;  /* Open waiters */
    wait_queue_head_t   close_wait; /* Close waiters */
    wait_queue_head_t   delta_msr_wait; /* Modem status change */
    unsigned long       flags;      /* TTY flags ASY_*/
    unsigned char       console:1,  /* port is a console */
                low_latency:1;  /* optional: tune for latency */
    struct mutex        mutex;      /* Locking */
    struct mutex        buf_mutex;  /* Buffer alloc lock */
    unsigned char       *xmit_buf;  /* Optional buffer */
    unsigned int        close_delay;    /* Close port delay */
    unsigned int        closing_wait;   /* Delay for output */
    int         drain_delay;    /* Set to zero if no pure time
                           based drain is needed else
                           set to size of fifo */
    struct kref     kref;       /* Ref counter */
};
struct tty_bufhead {
    struct tty_buffer *head;    /* Queue head */
    struct work_struct work;
    struct mutex       lock;
    atomic_t       priority;
    struct tty_buffer sentinel;
    struct llist_head free;     /* Free queue head */
    atomic_t       mem_used;    /* In-use buffers excluding free list */
    int        mem_limit;
    struct tty_buffer *tail;    /* Active buffer */
};
struct tty_buffer {
    union {
        struct tty_buffer *next;
        struct llist_node free;
    };
    int used;
    int size;
    int commit;
    int read;
    int flags;
    /* Data points here */
    unsigned long data[0];
};
