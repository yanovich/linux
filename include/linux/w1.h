#ifndef __LINUX_W1_H
#define __LINUX_W1_H

/* 64-bit 1-Wire device ID */
struct w1_reg_num
{
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u64	family:8,
		id:48,
		crc:8;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u64	crc:8,
		id:48,
		family:8;
#else
#error "Please fix <asm/byteorder.h>"
#endif
};

/* Events from the w1 core */
enum w1_netlink_message_types {
	W1_SLAVE_ADD = 0,
	W1_SLAVE_REMOVE,
	W1_MASTER_ADD,
	W1_MASTER_REMOVE,
	W1_MASTER_CMD,
	W1_SLAVE_CMD,
	W1_LIST_MASTERS,
};

#ifdef __KERNEL__

#include <linux/device.h>

#define W1_MAXNAMELEN		32

struct w1_slave
{
	struct module		*owner;
	unsigned char		name[W1_MAXNAMELEN];
	struct list_head	w1_slave_entry;
	struct w1_reg_num	reg_num;
	atomic_t		refcnt;
	u8			rom[9];
	u32			flags;
	int			ttl;

	struct w1_master	*master;
	struct w1_family	*family;
	void			*family_data;
	struct device		dev;
	struct completion	released;
};

typedef void (*w1_slave_found_callback)(struct w1_master *, u64);

/**
 * Note: read_bit and write_bit are very low level functions and should only
 * be used with hardware that doesn't really support 1-wire operations,
 * like a parallel/serial port.
 * Either define read_bit and write_bit OR define, at minimum, touch_bit and
 * reset_bus.
 */
struct w1_bus_master
{
	/** the first parameter in all the functions below */
	void		*data;

	/**
	 * Sample the line level
	 * @return the level read (0 or 1)
	 */
	u8		(*read_bit)(void *);

	/** Sets the line level */
	void		(*write_bit)(void *, u8);

	/**
	 * touch_bit is the lowest-level function for devices that really
	 * support the 1-wire protocol.
	 * touch_bit(0) = write-0 cycle
	 * touch_bit(1) = write-1 / read cycle
	 * @return the bit read (0 or 1)
	 */
	u8		(*touch_bit)(void *, u8);

	/**
	 * Reads a bytes. Same as 8 touch_bit(1) calls.
	 * @return the byte read
	 */
	u8		(*read_byte)(void *);

	/**
	 * Writes a byte. Same as 8 touch_bit(x) calls.
	 */
	void		(*write_byte)(void *, u8);

	/**
	 * Same as a series of read_byte() calls
	 * @return the number of bytes read
	 */
	u8		(*read_block)(void *, u8 *, int);

	/** Same as a series of write_byte() calls */
	void		(*write_block)(void *, const u8 *, int);

	/**
	 * Combines two reads and a smart write for ROM searches
	 * @return bit0=Id bit1=comp_id bit2=dir_taken
	 */
	u8		(*triplet)(void *, u8);

	/**
	 * long write-0 with a read for the presence pulse detection
	 * @return -1=Error, 0=Device present, 1=No device present
	 */
	u8		(*reset_bus)(void *);

	/**
	 * Put out a strong pull-up pulse of the specified duration.
	 * @return -1=Error, 0=completed
	 */
	u8		(*set_pullup)(void *, int);

	/** Really nice hardware can handles the different types of ROM search
	 *  w1_master* is passed to the slave found callback.
	 */
	void		(*search)(void *, struct w1_master *,
		u8, w1_slave_found_callback);
};

struct w1_master
{
	struct list_head	w1_master_entry;
	struct module		*owner;
	unsigned char		name[W1_MAXNAMELEN];
	struct list_head	slist;
	int			max_slave_count, slave_count;
	unsigned long		attempts;
	int			slave_ttl;
	int			initialized;
	u32			id;
	int			search_count;

	atomic_t		refcnt;

	void			*priv;
	int			priv_size;

	/** 5V strong pullup enabled flag, 1 enabled, zero disabled. */
	int			enable_pullup;
	/** 5V strong pullup duration in milliseconds, zero disabled. */
	int			pullup_duration;

	struct task_struct	*thread;
	struct mutex		mutex;
	struct mutex		bus_mutex;

	struct device_driver	*driver;
	struct device		dev;

	struct w1_bus_master	*bus_master;

	u32			seq;
};

#ifdef CONFIG_W1_NOTIFY

extern void w1_register_notify(struct notifier_block *nb);
extern void w1_unregister_notify(struct notifier_block *nb);

#endif  /* CONFIG_W1_NOTIFY */

#endif  /* __KERNEL__ */

#endif  /* __LINUX_W1_H */
