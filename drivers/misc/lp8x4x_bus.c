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

static struct attribute *master_dev_attrs[] = {
	&dev_attr_slot_count.attr,
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

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "could not get slot count address\n");
		err = -ENODEV;
		goto err2;
	}

	m->count_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(m->count_addr)) {
		dev_err(&pdev->dev, "Failed to ioremap slot count address\n");
		err = PTR_ERR(m->count_addr);
		goto err2;
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
		goto err2;
	};

	dev_info(&pdev->dev, "found bus with up to %u slots\n", m->slot_count);

	err = bus_register(&lp8x4x_bus_type);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register bus type\n");
		goto err2;
	}

	m->dev.bus = &lp8x4x_bus_type;
	dev_set_name(&m->dev, "backplane");
	m->dev.parent = &pdev->dev;
	m->dev.release = lp8x4x_master_release;
	m->dev.groups = master_dev_groups;

	err = device_register(&m->dev);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register backplane device\n");
		goto err3;
	}

	devres_add(&pdev->dev, p);
	return 0;

err3:
	bus_unregister(&lp8x4x_bus_type);
err2:
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
