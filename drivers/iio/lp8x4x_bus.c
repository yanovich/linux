/*
 *  linux/drivers/iio/lp8x4x_bus.c
 *
 *  Support for ICP DAS LP-8x4x programmable automation controller bus
 *  Copyright (C) 2013 Sergey Yanovich
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation or any later version.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/w1.h>
#include <linux/w1-gpio.h>
#include <linux/platform_device.h>

#include <mach/mfp-pxa27x.h>
#include <asm/system_info.h>

#define MODULE_NAME	"lp8x4x_bus"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sergey Yanovich <ynvich@gmail.com>");
MODULE_DESCRIPTION("ICP DAS LP-8x4x parallel bus driver");

static void lp8x4x_noop_release(struct device *dev) {}

static int lp8x4x_w1_notify(struct notifier_block *nb,
	       	unsigned long state, void *dev)
{
	struct w1_slave *sl;
	u8 sn[8];
	u64 id;
	if (state != W1_SLAVE_ADD)
		return NOTIFY_DONE;

	sl = (struct w1_slave *) dev;
	sn[7] = sl->reg_num.family;
	id = sl->reg_num.id;
	memcpy(&sn[1], &id, 6);
	sn[0] = sl->reg_num.crc;
	id = *(u64 *) sn;
	system_serial_high = (unsigned int) (id >> 32);
	system_serial_low = (unsigned int) (id & 0xFFFFFFFF);
	printk(KERN_INFO MODULE_NAME ": LP-8x4x serial %016llx\n", id);

	return NOTIFY_DONE;
}

static struct w1_gpio_platform_data lp8x4x_w1_gpio_data = {
	.pin			= MFP_PIN_GPIO83,
	.is_open_drain		= 0,
	.ext_pullup_enable_pin	= -1,
};

static struct platform_device lp8x4x_w1_master_device = {
	.name	= "w1-gpio",
	.dev	= {
		.platform_data = &lp8x4x_w1_gpio_data,
		.release = lp8x4x_noop_release,
	}
};      

struct notifier_block lp8x4x_notifier = {
	.notifier_call = lp8x4x_w1_notify,
};

static int __init lp8x4x_bus_init(void)
{
	platform_device_register(&lp8x4x_w1_master_device);
	w1_register_notify(&lp8x4x_notifier);
	printk(KERN_INFO MODULE_NAME ": loaded\n");
	return 0;
}

static void __exit lp8x4x_bus_exit(void)
{
	w1_unregister_notify(&lp8x4x_notifier);
	platform_device_unregister(&lp8x4x_w1_master_device);
	printk(KERN_INFO MODULE_NAME ": unloaded\n");
}

module_init(lp8x4x_bus_init);
module_exit(lp8x4x_bus_exit);
