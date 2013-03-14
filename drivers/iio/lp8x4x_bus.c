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

#define MODULE_NAME	"lp8x4x_bus"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sergey Yanovich <ynvich@gmail.com>");
MODULE_DESCRIPTION("ICP DAS LP-8x4x parallel bus driver");

static int __init lp8x4x_bus_init(void)
{
	printk(KERN_INFO MODULE_NAME ": loaded\n");
	return 0;
}

static void __exit lp8x4x_bus_exit(void)
{
	printk(KERN_INFO MODULE_NAME ": unloaded\n");
}

module_init(lp8x4x_bus_init);
module_exit(lp8x4x_bus_exit);
