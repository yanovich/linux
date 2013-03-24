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
#include <linux/list.h>
#include <linux/slab.h>

#include <mach/mfp-pxa27x.h>
#include <mach/lp8x4x.h>
#include <asm/system_info.h>

#define MODULE_NAME	"lp8x4x_bus"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sergey Yanovich <ynvich@gmail.com>");
MODULE_DESCRIPTION("ICP DAS LP-8x4x parallel bus driver");

struct lp8x4x_module {
	unsigned long		addr;
};

struct lp8x4x_module_device {
	struct list_head        slot_entry;
	unsigned int		model;
	struct device		dev;
};

struct lp8x4x_bus_device {
	struct mutex            mutex;
	struct list_head        slots;
	struct device		dev;
};

static void lp8x4x_noop_release(struct device *dev) {}

#define LP8X4X_MAX_MODULE_COUNT		8
static unsigned int lp8x4x_slot_count;

static unsigned char lp8x4x_model[256] = {
	   0,    0,    0, 0x11,    0, 0x18, 0x13, 0x11,
	0x0e, 0x11,    0,    0,    0, 0x5a, 0x5b, 0x5c,
	0x3c, 0x44, 0x34, 0x3a, 0x39, 0x36, 0x37, 0x33,
	0x35, 0x40, 0x41, 0x42, 0x38, 0x3f, 0x32, 0x45,
	0xac, 0x70, 0x8e, 0x8e, 0x1e, 0x72, 0x90, 0x29,
	0x4a, 0x22, 0xd3, 0xd2, 0x28, 0x25, 0x2a, 0x29,
	0x48, 0x49, 0x5d, 0x1f, 0x20, 0x23, 0x24, 0x4d,
	0x3d, 0x3e,    0,    0,    0,    0,    0,    0,
	   0, 0x78, 0x72, 0x2b, 0x5e, 0x5e, 0x36, 0xae,
	0x30,    0,    0,    0,    0,    0,    0,    0,
	   0,    0, 0x5c, 0x5e,    0, 0x5e,    0,    0,
	   0, 0x3b,    0,    0,    0,    0,    0,    0,
	   0, 0x50, 0x2e,    0, 0x58,    0,    0, 0x43,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0, 0x54,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0
};

struct lp8x4x_module lp8x4x_module[LP8X4X_MAX_MODULE_COUNT] = {
	{
		.addr 		= LP8X4X_P2V (0x17001000),
	},
	{
		.addr 		= LP8X4X_P2V (0x17002000),
	},
	{
		.addr 		= LP8X4X_P2V (0x17003000),
	},
	{
		.addr 		= LP8X4X_P2V (0x17004000),
	},
	{
		.addr 		= LP8X4X_P2V (0x17005000),
	},
	{
		.addr 		= LP8X4X_P2V (0x17006000),
	},
	{
		.addr 		= LP8X4X_P2V (0x17007000),
	},
	{
		.addr 		= LP8X4X_P2V (0x17008000),
	}
};

static int lp8x4x_match(struct device *dev, struct device_driver *drv)
{
	return 1;
}

struct bus_type lp8x4x_bus_type = {
	.name		= "icpdas",
	.match		= lp8x4x_match,
};

struct lp8x4x_bus_device lp8x4x_bus = {
	.dev		= {
		.bus		= &lp8x4x_bus_type,
		.init_name	= "backplane",
		.release	= &lp8x4x_noop_release,
	},
};

static ssize_t lp8x4x_model_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lp8x4x_module_device *mdev =
		container_of(dev, struct lp8x4x_module_device, dev);

	return sprintf(buf, "%u\n", mdev->model + 8000);
}

static DEVICE_ATTR(model, S_IRUGO, lp8x4x_model_show, NULL);

static void lp8x4x_module_release(struct device *dev)
{
	struct lp8x4x_module_device *mdev =
		container_of(dev, struct lp8x4x_module_device, dev);

	kfree(mdev);
}

static void lp8x4x_module_attach(int i, unsigned long model)
{
	int err;
	struct lp8x4x_module_device *mdev;

	printk(KERN_INFO MODULE_NAME ": found %lu in slot %i\n",
			8000 + model, i + 1);

	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev) {
		dev_err(&lp8x4x_bus.dev, "failed to allocate device\n");
		return;
	}

	mdev->dev.id = i;
	mdev->model = model;
	mdev->dev.parent = &lp8x4x_bus.dev;
	mdev->dev.bus = &lp8x4x_bus_type;
	mdev->dev.release = &lp8x4x_module_release;
	dev_set_name(&mdev->dev, "slot%02i", i);
	err = device_register(&mdev->dev);
	if (err < 0) {
		dev_err(&lp8x4x_bus.dev, "failed to register slot %02i\n", i);
		return;
	}

	err = device_create_file(&mdev->dev, &dev_attr_model);
	if (err < 0) {
		dev_err(&lp8x4x_bus.dev,
			       	"failed to create attr for slot %02i\n", i);
		goto module_unreg;
	}

	mutex_lock(&lp8x4x_bus.mutex);
	list_add_tail(&mdev->slot_entry, &lp8x4x_bus.slots);
	mutex_unlock(&lp8x4x_bus.mutex);
	return;

module_unreg:
	device_unregister(&mdev->dev);
}

/* Should hold mutex */
static void lp8x4x_module_detach(struct lp8x4x_module_device *mdev)
{
	list_del(&mdev->slot_entry);
	device_unregister(&mdev->dev);
}

static ssize_t lp8x4x_slot_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", lp8x4x_slot_count);
}

static DEVICE_ATTR(slot_count, S_IRUGO, lp8x4x_slot_count_show, NULL);

static void lp8x4x_init_bus(struct device *device)
{
	int i, err;
	unsigned long model;

	INIT_LIST_HEAD(&lp8x4x_bus.slots);
	mutex_init(&lp8x4x_bus.mutex);
	lp8x4x_bus.dev.parent = device;

	err = device_register(&lp8x4x_bus.dev);
	if (err < 0) {
		printk(KERN_ERR MODULE_NAME ": device_register failed\n");
		return;
	}

	lp8x4x_slot_count = LP8X4X_MOD_NUM & 0xff;
	printk(KERN_INFO MODULE_NAME ": up to %u modules\n", lp8x4x_slot_count);

	err = device_create_file(&lp8x4x_bus.dev, &dev_attr_slot_count);
	if (err < 0) {
		printk(KERN_ERR MODULE_NAME ": device_create_file failed\n");
		goto device_unreg;
	}

	for (i = 0; i < lp8x4x_slot_count; i++) {
		model = lp8x4x_model[__LP8X4X_MEM(lp8x4x_module[i].addr)
		       	& 0xff];
		if (!model)
			continue;

		lp8x4x_module_attach(i, model);
	}
	return;

device_unreg:
	device_unregister(&lp8x4x_bus.dev);
	lp8x4x_bus.dev.parent = NULL;
}

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

	lp8x4x_init_bus(&sl->dev);
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
	int err;

	err = bus_register(&lp8x4x_bus_type);
	if (err < 0)
		return err;

	err = platform_device_register(&lp8x4x_w1_master_device);
	if (err < 0)
		goto bus_unreg;

	w1_register_notify(&lp8x4x_notifier);

	printk(KERN_INFO MODULE_NAME ": loaded\n");
	return 0;

bus_unreg:
	bus_unregister(&lp8x4x_bus_type);

	return err;
}

static void __exit lp8x4x_bus_exit(void)
{
	struct lp8x4x_module_device *m, *mn;

	if (lp8x4x_bus.dev.parent) {
		mutex_lock(&lp8x4x_bus.mutex);
		list_for_each_entry_safe(m, mn, &lp8x4x_bus.slots, slot_entry)
			lp8x4x_module_detach(m);
		mutex_unlock(&lp8x4x_bus.mutex);
		device_unregister(&lp8x4x_bus.dev);
	}
	w1_unregister_notify(&lp8x4x_notifier);
	platform_device_unregister(&lp8x4x_w1_master_device);
	bus_unregister(&lp8x4x_bus_type);
	printk(KERN_INFO MODULE_NAME ": unloaded\n");
}

module_init(lp8x4x_bus_init);
module_exit(lp8x4x_bus_exit);
