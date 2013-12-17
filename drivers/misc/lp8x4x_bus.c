/*
 *  linux/misc/lp8x4x_bus.c
 *
 *  Support for ICP DAS LP-8x4x programmable automation controller bus
 *  Copyright (C) 2013 Sergei Ianovich <ynvich@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation or any later version.
 */
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define MODULE_NAME	"lp8x4x-bus"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sergei Ianovich <ynvich@gmail.com>");
MODULE_DESCRIPTION("ICP DAS LP-8x4x parallel bus driver");

struct lp8x4x_master {
	unsigned int		slot_count;
	void			*count_addr;
	void			*rotary_addr;
	void			*dip_addr;
	struct gpio_desc        *eeprom_nWE;
	struct device		dev;
};

static int lp8x4x_match(struct device *dev, struct device_driver *drv)
{
	return 1;
}

static struct bus_type lp8x4x_bus_type = {
	.name		= "icpdas",
	.match		= lp8x4x_match,
};

static void lp8x4x_master_release(struct device *dev)
{
	struct lp8x4x_master *m = container_of(dev, struct lp8x4x_master, dev);
	WARN_ON(!dev);

	kfree(m);
}

static ssize_t slot_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lp8x4x_master *m = container_of(dev, struct lp8x4x_master, dev);

	return sprintf(buf, "%u\n", m->slot_count);
}

static DEVICE_ATTR_RO(slot_count);

static ssize_t rotary_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lp8x4x_master *m = container_of(dev, struct lp8x4x_master, dev);

	return sprintf(buf, "%u\n", (ioread8(m->rotary_addr) ^ 0xf) & 0xf);
}

static DEVICE_ATTR_RO(rotary);

static ssize_t eeprom_write_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lp8x4x_master *m = container_of(dev, struct lp8x4x_master, dev);

	return sprintf(buf, "%u\n", !gpiod_get_value(m->eeprom_nWE));
}

static ssize_t eeprom_write_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct lp8x4x_master *m = container_of(dev, struct lp8x4x_master, dev);
	unsigned int val = 0;
	int err;

	if (!buf)
		return count;

	if (0 == count)
		return count;

	err = kstrtouint(buf, 10, &val);
	if (err != 0) {
		dev_err(dev, "Bad input %s\n", buf);
		return count;
	}

	gpiod_set_value(m->eeprom_nWE, !val);

	return count;
}

static DEVICE_ATTR_RW(eeprom_write_enable);

static ssize_t dip_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lp8x4x_master *m = container_of(dev, struct lp8x4x_master, dev);

	return sprintf(buf, "0x%02x\n", ioread8(m->dip_addr) ^ 0xff);
}

static DEVICE_ATTR_RO(dip);

static struct attribute *master_dev_attrs[] = {
	&dev_attr_slot_count.attr,
	&dev_attr_rotary.attr,
	&dev_attr_eeprom_write_enable.attr,
	&dev_attr_dip.attr,
	NULL,
};
ATTRIBUTE_GROUPS(master_dev);


static void devm_lp8x4x_bus_release(struct device *dev, void *res)
{
	struct lp8x4x_master *m = *(struct lp8x4x_master **)res;

	dev_dbg(dev, "releasing devices\n");
	device_unregister(&m->dev);
	bus_unregister(&lp8x4x_bus_type);
}

static int __init lp8x4x_bus_probe(struct platform_device *pdev)
{
	struct lp8x4x_master *m, **p;
	struct resource *res;
	int r = 0;
	int err = 0;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

	p = devres_alloc(devm_lp8x4x_bus_release, sizeof(*p), GFP_KERNEL);
	if (!p) {
		err = -ENOMEM;
		goto err1;
	}
	*p = m;

	res = platform_get_resource(pdev, IORESOURCE_MEM, r++);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get rotary address\n");
		err = -ENODEV;
		goto err_free;
	}

	m->rotary_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(m->rotary_addr)) {
		dev_err(&pdev->dev, "Failed to ioremap rotary address\n");
		err = PTR_ERR(m->rotary_addr);
		goto err_free;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, r++);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get DIP switch address\n");
		err = -ENODEV;
		goto err_free;
	}

	m->dip_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(m->dip_addr)) {
		dev_err(&pdev->dev, "Failed to ioremap DIP switch address\n");
		err = PTR_ERR(m->dip_addr);
		goto err_free;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, r++);
	if (!res) {
		dev_err(&pdev->dev, "could not get slot count address\n");
		err = -ENODEV;
		goto err_free;
	}

	m->count_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(m->count_addr)) {
		dev_err(&pdev->dev, "Failed to ioremap slot count address\n");
		err = PTR_ERR(m->count_addr);
		goto err_free;
	}

	m->eeprom_nWE = devm_gpiod_get(&pdev->dev, "eeprom");
	if (IS_ERR(m->eeprom_nWE)) {
		err = PTR_ERR(m->eeprom_nWE);
		dev_err(&pdev->dev, "Failed to get eeprom GPIO\n");
		goto err_free;
	}

	err = gpiod_direction_output(m->eeprom_nWE, 1);
	if (err < 0) {
		dev_err(&pdev->dev, "Failed to set eeprom GPIO output\n");
		goto err_free;
	}

	m->slot_count = ioread8(m->count_addr);
	switch (m->slot_count) {
	case 1:
	case 4:
		break;
	case 7:
		m->slot_count = 8;
		break;
	default:
		dev_err(&pdev->dev, "unexpected slot number(%u)",
				m->slot_count);
		err = -ENODEV;
		goto err_free;
	};

	dev_info(&pdev->dev, "found bus with up to %u slots\n", m->slot_count);

	err = bus_register(&lp8x4x_bus_type);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register bus type\n");
		goto err_free;
	}

	m->dev.bus = &lp8x4x_bus_type;
	dev_set_name(&m->dev, "backplane");
	m->dev.parent = &pdev->dev;
	m->dev.release = lp8x4x_master_release;
	m->dev.groups = master_dev_groups;

	err = device_register(&m->dev);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register backplane device\n");
		goto err_bus;
	}

	devres_add(&pdev->dev, p);
	return 0;

err_bus:
	bus_unregister(&lp8x4x_bus_type);
err_free:
	devres_free(p);
err1:
	kfree(m);
	return err;
}

static const struct of_device_id lp8x4x_bus_dt_ids[] = {
	{ .compatible = "icpdas,backplane-lp8x4x" },
	{ }
};
MODULE_DEVICE_TABLE(of, lp8x4x_bus_dt_ids);

static struct platform_driver lp8x4x_bus_driver = {
	.driver		= {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = lp8x4x_bus_dt_ids,
	},
};

module_platform_driver_probe(lp8x4x_bus_driver, lp8x4x_bus_probe);
