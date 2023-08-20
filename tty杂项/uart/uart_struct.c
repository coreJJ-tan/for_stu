/*
 * This structure describes all the operations that can be done on the
 * physical hardware.  See Documentation/serial/driver for details.
 */
struct uart_ops {
    unsigned int    (*tx_empty)(struct uart_port *);
    void        (*set_mctrl)(struct uart_port *, unsigned int mctrl);
    unsigned int    (*get_mctrl)(struct uart_port *);
    void        (*stop_tx)(struct uart_port *);
    void        (*start_tx)(struct uart_port *);
    void        (*throttle)(struct uart_port *);
    void        (*unthrottle)(struct uart_port *);
    void        (*send_xchar)(struct uart_port *, char ch);
    void        (*stop_rx)(struct uart_port *);
    void        (*enable_ms)(struct uart_port *);
    void        (*break_ctl)(struct uart_port *, int ctl);
    int     (*startup)(struct uart_port *);
    void        (*shutdown)(struct uart_port *);
    void        (*flush_buffer)(struct uart_port *);
    void        (*set_termios)(struct uart_port *, struct ktermios *new,
                       struct ktermios *old);
    void        (*set_ldisc)(struct uart_port *, struct ktermios *);
    void        (*pm)(struct uart_port *, unsigned int state,
                  unsigned int oldstate);

    /*
     * Return a string describing the type of the port
     */
    const char  *(*type)(struct uart_port *);

    /*
     * Release IO and memory resources used by the port.
     * This includes iounmap if necessary.
     */
    void        (*release_port)(struct uart_port *);

    /*
     * Request IO and memory resources used by the port.
     * This includes iomapping the port if necessary.
     */
    int     (*request_port)(struct uart_port *);
    void        (*config_port)(struct uart_port *, int);
    int     (*verify_port)(struct uart_port *, struct serial_struct *);
    int     (*ioctl)(struct uart_port *, unsigned int, unsigned long);
#ifdef CONFIG_CONSOLE_POLL
    int     (*poll_init)(struct uart_port *);
    void        (*poll_put_char)(struct uart_port *, unsigned char);
    int     (*poll_get_char)(struct uart_port *);
#endif
};


struct uart_port {
    spinlock_t      lock;           /* port lock */
    unsigned long       iobase;         /* in/out[bwl] */
    unsigned char __iomem   *membase;       /* read/write[bwl] */
    unsigned int        (*serial_in)(struct uart_port *, int);
    void            (*serial_out)(struct uart_port *, int, int);
    void            (*set_termios)(struct uart_port *,
                               struct ktermios *new,
                               struct ktermios *old);
    void            (*set_mctrl)(struct uart_port *, unsigned int);
    int         (*startup)(struct uart_port *port);
    void            (*shutdown)(struct uart_port *port);
    void            (*throttle)(struct uart_port *port);
    void            (*unthrottle)(struct uart_port *port);
    int         (*handle_irq)(struct uart_port *);
    void            (*pm)(struct uart_port *, unsigned int state,
                      unsigned int old);
    void            (*handle_break)(struct uart_port *);
    int         (*rs485_config)(struct uart_port *,
                        struct serial_rs485 *rs485);
    unsigned int        irq;            /* irq number */
    unsigned long       irqflags;       /* irq flags  */
    unsigned int        uartclk;        /* base uart clock */
    unsigned int        fifosize;       /* tx fifo size */
    unsigned char       x_char;         /* xon/xoff char */
    unsigned char       regshift;       /* reg offset shift */
    unsigned char       iotype;         /* io access style */
    unsigned char       unused1;

#define UPIO_PORT       (SERIAL_IO_PORT)    /* 8b I/O port access */
#define UPIO_HUB6       (SERIAL_IO_HUB6)    /* Hub6 ISA card */
#define UPIO_MEM        (SERIAL_IO_MEM)     /* 8b MMIO access */
#define UPIO_MEM32      (SERIAL_IO_MEM32)   /* 32b little endian */
#define UPIO_AU         (SERIAL_IO_AU)      /* Au1x00 and RT288x type IO */
#define UPIO_TSI        (SERIAL_IO_TSI)     /* Tsi108/109 type IO */
#define UPIO_MEM32BE        (SERIAL_IO_MEM32BE) /* 32b big endian */

    unsigned int        read_status_mask;   /* driver specific */
    unsigned int        ignore_status_mask; /* driver specific */
    struct uart_state   *state;         /* pointer to parent state */
    struct uart_icount  icount;         /* statistics */

    struct console      *cons;          /* struct console, if any */
#if defined(CONFIG_SERIAL_CORE_CONSOLE) || defined(SUPPORT_SYSRQ)
    unsigned long       sysrq;          /* sysrq timeout */
#endif

    /* flags must be updated while holding port mutex */
    upf_t           flags;

    /*
     * These flags must be equivalent to the flags defined in
     * include/uapi/linux/tty_flags.h which are the userspace definitions
     * assigned from the serial_struct flags in uart_set_info()
     * [for bit definitions in the UPF_CHANGE_MASK]
     *
     * Bits [0..UPF_LAST_USER] are userspace defined/visible/changeable
     * except bit 15 (UPF_NO_TXEN_TEST) which is masked off.
     * The remaining bits are serial-core specific and not modifiable by
     * userspace.
     */
#define UPF_FOURPORT        ((__force upf_t) ASYNC_FOURPORT       /* 1  */ )
#define UPF_SAK         ((__force upf_t) ASYNC_SAK            /* 2  */ )
#define UPF_SPD_HI      ((__force upf_t) ASYNC_SPD_HI         /* 4  */ )
#define UPF_SPD_VHI     ((__force upf_t) ASYNC_SPD_VHI        /* 5  */ )
#define UPF_SPD_CUST        ((__force upf_t) ASYNC_SPD_CUST   /* 0x0030 */ )
#define UPF_SPD_WARP        ((__force upf_t) ASYNC_SPD_WARP   /* 0x1010 */ )
#define UPF_SPD_MASK        ((__force upf_t) ASYNC_SPD_MASK   /* 0x1030 */ )
#define UPF_SKIP_TEST       ((__force upf_t) ASYNC_SKIP_TEST      /* 6  */ )
#define UPF_AUTO_IRQ        ((__force upf_t) ASYNC_AUTO_IRQ       /* 7  */ )
#define UPF_HARDPPS_CD      ((__force upf_t) ASYNC_HARDPPS_CD     /* 11 */ )
#define UPF_SPD_SHI     ((__force upf_t) ASYNC_SPD_SHI        /* 12 */ )
#define UPF_LOW_LATENCY     ((__force upf_t) ASYNC_LOW_LATENCY    /* 13 */ )
#define UPF_BUGGY_UART      ((__force upf_t) ASYNC_BUGGY_UART     /* 14 */ )
#define UPF_NO_TXEN_TEST    ((__force upf_t) (1 << 15))
#define UPF_MAGIC_MULTIPLIER    ((__force upf_t) ASYNC_MAGIC_MULTIPLIER /* 16 */ )

/* Port has hardware-assisted h/w flow control */
#define UPF_AUTO_CTS        ((__force upf_t) (1 << 20))
#define UPF_AUTO_RTS        ((__force upf_t) (1 << 21))
#define UPF_HARD_FLOW       ((__force upf_t) (UPF_AUTO_CTS | UPF_AUTO_RTS))
/* Port has hardware-assisted s/w flow control */
#define UPF_SOFT_FLOW       ((__force upf_t) (1 << 22))
#define UPF_CONS_FLOW       ((__force upf_t) (1 << 23))
#define UPF_SHARE_IRQ       ((__force upf_t) (1 << 24))
#define UPF_EXAR_EFR        ((__force upf_t) (1 << 25))
#define UPF_BUG_THRE        ((__force upf_t) (1 << 26))
/* The exact UART type is known and should not be probed.  */
#define UPF_FIXED_TYPE      ((__force upf_t) (1 << 27))
#define UPF_BOOT_AUTOCONF   ((__force upf_t) (1 << 28))
#define UPF_FIXED_PORT      ((__force upf_t) (1 << 29))
#define UPF_DEAD        ((__force upf_t) (1 << 30))
#define UPF_IOREMAP     ((__force upf_t) (1 << 31))

#define __UPF_CHANGE_MASK   0x17fff
#define UPF_CHANGE_MASK     ((__force upf_t) __UPF_CHANGE_MASK)
#define UPF_USR_MASK        ((__force upf_t) (UPF_SPD_MASK|UPF_LOW_LATENCY))

#if __UPF_CHANGE_MASK > ASYNC_FLAGS
#error Change mask not equivalent to userspace-visible bit defines
#endif

    /*
     * Must hold termios_rwsem, port mutex and port lock to change;
     * can hold any one lock to read.
     */
    upstat_t        status;

#define UPSTAT_CTS_ENABLE   ((__force upstat_t) (1 << 0))
#define UPSTAT_DCD_ENABLE   ((__force upstat_t) (1 << 1))
#define UPSTAT_AUTORTS      ((__force upstat_t) (1 << 2))
#define UPSTAT_AUTOCTS      ((__force upstat_t) (1 << 3))
#define UPSTAT_AUTOXOFF     ((__force upstat_t) (1 << 4))

    int         hw_stopped;     /* sw-assisted CTS flow state */
    unsigned int        mctrl;          /* current modem ctrl settings */
    unsigned int        timeout;        /* character-based timeout */
    unsigned int        type;           /* port type */
    const struct uart_ops   *ops;
    unsigned int        custom_divisor;
    unsigned int        line;           /* port index */
    unsigned int        minor;
    resource_size_t     mapbase;        /* for ioremap */
    resource_size_t     mapsize;
    struct device       *dev;           /* parent device */
    unsigned char       hub6;           /* this should be in the 8250 driver */
    unsigned char       suspended;
    unsigned char       irq_wake;
    unsigned char       unused[2];
    struct attribute_group  *attr_group;        /* port specific attributes */
    const struct attribute_group **tty_groups;  /* all attributes (serial core use only) */
    struct serial_rs485     rs485;
    void            *private_data;      /* generic platform data pointer */
};



/*
 * This is the state information which is persistent across opens.
 */
struct uart_state {  // 这个结构体不应该在 low level driver (uart 驱动层 是一种 low level driver) 中注册, 它在 uart_register_driver() 时会由 uart 核心层注册
    struct tty_port     port;

    enum uart_pm_state  pm_state;
    struct circ_buf     xmit;

    struct uart_port    *uart_port;
};



struct uart_driver {
    struct module       *owner;
    const char      *driver_name;
    const char      *dev_name;
    int          major;
    int          minor;
    int          nr;
    struct console      *cons;

    /*
     * these are private; the low level driver should not
     * touch these; they should be initialised to NULL
     */
    struct uart_state   *state;
    struct tty_driver   *tty_driver;
};



struct tty_driver {
    int magic;      /* magic number for this structure */
    struct kref kref;   /* Reference management */
    struct cdev *cdevs;
    struct module   *owner;
    const char  *driver_name;
    const char  *name;
    int name_base;  /* offset of printed name */
    int major;      /* major device number */
    int minor_start;    /* start of minor device number */
    unsigned int    num;    /* number of devices allocated */
    short   type;       /* type of tty driver */
    short   subtype;    /* subtype of tty driver */
    struct ktermios init_termios; /* Initial termios */
    unsigned long   flags;      /* tty driver flags */
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

struct tty_operations {
    struct tty_struct * (*lookup)(struct tty_driver *driver,
            struct inode *inode, int idx);
    int  (*install)(struct tty_driver *driver, struct tty_struct *tty);
    void (*remove)(struct tty_driver *driver, struct tty_struct *tty);
    int  (*open)(struct tty_struct * tty, struct file * filp);
    void (*close)(struct tty_struct * tty, struct file * filp);
    void (*shutdown)(struct tty_struct *tty);
    void (*cleanup)(struct tty_struct *tty);
    int  (*write)(struct tty_struct * tty,
              const unsigned char *buf, int count);
    int  (*put_char)(struct tty_struct *tty, unsigned char ch);
    void (*flush_chars)(struct tty_struct *tty);
    int  (*write_room)(struct tty_struct *tty);
    int  (*chars_in_buffer)(struct tty_struct *tty);
    int  (*ioctl)(struct tty_struct *tty,
            unsigned int cmd, unsigned long arg);
    long (*compat_ioctl)(struct tty_struct *tty,
                 unsigned int cmd, unsigned long arg);
    void (*set_termios)(struct tty_struct *tty, struct ktermios * old);
    void (*throttle)(struct tty_struct * tty);
    void (*unthrottle)(struct tty_struct * tty);
    void (*stop)(struct tty_struct *tty);
    void (*start)(struct tty_struct *tty);
    void (*hangup)(struct tty_struct *tty);
    int (*break_ctl)(struct tty_struct *tty, int state);
    void (*flush_buffer)(struct tty_struct *tty);
    void (*set_ldisc)(struct tty_struct *tty);
    void (*wait_until_sent)(struct tty_struct *tty, int timeout);
    void (*send_xchar)(struct tty_struct *tty, char ch);
    int (*tiocmget)(struct tty_struct *tty);
    int (*tiocmset)(struct tty_struct *tty,
            unsigned int set, unsigned int clear);
    int (*resize)(struct tty_struct *tty, struct winsize *ws);
    int (*set_termiox)(struct tty_struct *tty, struct termiox *tnew);
    int (*get_icount)(struct tty_struct *tty,
                struct serial_icounter_struct *icount);
#ifdef CONFIG_CONSOLE_POLL
    int (*poll_init)(struct tty_driver *driver, int line, char *options);
    int (*poll_get_char)(struct tty_driver *driver, int line);
    void (*poll_put_char)(struct tty_driver *driver, int line, char ch);
#endif
    const struct file_operations *proc_fops;
};


struct tty_port_operations {
    /* Return 1 if the carrier is raised */
    int (*carrier_raised)(struct tty_port *port);
    /* Control the DTR line */
    void (*dtr_rts)(struct tty_port *port, int raise);
    /* Called when the last close completes or a hangup finishes
       IFF the port was initialized. Do not use to free resources. Called
       under the port mutex to serialize against activate/shutdowns */
    void (*shutdown)(struct tty_port *port);
    /* Called under the port mutex from tty_port_open, serialized using
       the port mutex */
        /* FIXME: long term getting the tty argument *out* of this would be
           good for consoles */
    int (*activate)(struct tty_port *port, struct tty_struct *tty);
    /* Called on the final put of a port */
    void (*destruct)(struct tty_port *port);
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
