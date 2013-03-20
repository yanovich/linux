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
#include <linux/w1-gpio.h>
#include <linux/platform_device.h>

#include <mach/mfp-pxa27x.h>

#define MODULE_NAME	"lp8x4x_bus"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sergey Yanovich <ynvich@gmail.com>");
MODULE_DESCRIPTION("ICP DAS LP-8x4x parallel bus driver");

static void lp8x4x_noop_release(struct device *dev) {}

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

static int __init lp8x4x_bus_init(void)
{
	platform_device_register(&lp8x4x_w1_master_device);
	printk(KERN_INFO MODULE_NAME ": loaded\n");
	return 0;
}

static void __exit lp8x4x_bus_exit(void)
{
	platform_device_unregister(&lp8x4x_w1_master_device);
	printk(KERN_INFO MODULE_NAME ": unloaded\n");
}

module_init(lp8x4x_bus_init);
module_exit(lp8x4x_bus_exit);
