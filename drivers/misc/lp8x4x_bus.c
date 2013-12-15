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

struct lp8x4x_slot {
	void			*data_addr;
	unsigned int		model;
	struct device		dev;
};

#define LP8X4X_MAX_SLOT_COUNT	8
struct lp8x4x_master {
	unsigned int		slot_count;
	void			*count_addr;
	void			*rotary_addr;
	void			*dip_addr;
	struct gpio_desc        *eeprom_nWE;
	unsigned int		active_slot;
	void			*switch_addr;
	struct lp8x4x_slot	slot[LP8X4X_MAX_SLOT_COUNT];
	struct device		dev;
};

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

static int lp8x4x_match(struct device *dev, struct device_driver *drv)
{
	return 1;
}

static struct bus_type lp8x4x_bus_type = {
	.name		= "icpdas",
	.match		= lp8x4x_match,
};

static void lp8x4x_slot_release(struct device *dev)
{
}

static ssize_t model_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lp8x4x_slot *s = container_of(dev, struct lp8x4x_slot, dev);

	return sprintf(buf, "%u\n", s->model + 8000);
}

static DEVICE_ATTR_RO(model);

static struct attribute *slot_dev_attrs[] = {
	&dev_attr_model.attr,
	NULL,
};
ATTRIBUTE_GROUPS(slot_dev);

static void lp8x4x_master_release(struct device *dev)
{
	struct lp8x4x_master *m = container_of(dev, struct lp8x4x_master, dev);
	WARN_ON(!dev);

	/* Disable serial communications */
	iowrite8(0xff, m->switch_addr);

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

static ssize_t active_slot_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lp8x4x_master *m = container_of(dev, struct lp8x4x_master, dev);

	return sprintf(buf, "%u\n", m->active_slot);
}

static ssize_t active_slot_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct lp8x4x_master *m = container_of(dev, struct lp8x4x_master, dev);
	unsigned int active_slot = 0;
	int err;

	if (!buf)
		return count;
	if (0 == count)
		return count;

	err = kstrtouint(buf, 10, &active_slot);
	if (err != 0 || active_slot > m->slot_count) {
		dev_err(dev, "slot number is out of range 1..%u\n",
				m->slot_count);
		return count;
	}

	if (!active_slot) {
		m->active_slot = 0;
		iowrite8(0xff, m->switch_addr);
		return count;
	}

	m->active_slot = active_slot;
	iowrite8((1 << (m->active_slot - 1)) ^ 0xff, m->switch_addr);
	return count;
}

static DEVICE_ATTR_RW(active_slot);

static struct attribute *master_dev_attrs[] = {
	&dev_attr_slot_count.attr,
	&dev_attr_rotary.attr,
	&dev_attr_eeprom_write_enable.attr,
	&dev_attr_dip.attr,
	&dev_attr_active_slot.attr,
	NULL,
};
ATTRIBUTE_GROUPS(master_dev);


static void devm_lp8x4x_bus_release(struct device *dev, void *res)
{
	struct lp8x4x_master *m = *(struct lp8x4x_master **)res;
	struct lp8x4x_slot *s;
	int i;

	dev_dbg(dev, "releasing devices\n");
	for (i = 0; i < LP8X4X_MAX_SLOT_COUNT; i++) {
		s = &m->slot[i];
		if (s->model)
			device_unregister(&s->dev);
	}
	device_unregister(&m->dev);
	bus_unregister(&lp8x4x_bus_type);
}

static void __init lp8x4x_bus_probe_slot(struct lp8x4x_master *m, int i,
		unsigned char model)
{
	struct lp8x4x_slot *s = &m->slot[i];
	int err;

	dev_info(&m->dev, "found %u in slot %i\n", 8000 + model, i + 1);

	s->dev.bus = &lp8x4x_bus_type;
	dev_set_name(&s->dev, "slot%02i", i + 1);
	s->dev.parent = &m->dev;
	s->dev.release = lp8x4x_slot_release;
	s->dev.groups = slot_dev_groups;
	s->model = model;

	err = device_register(&s->dev);
	if (err < 0) {
		dev_err(&s->dev, "failed to register device\n");
		s->model = 0;
		return;
	}
}

static int __init lp8x4x_bus_probe(struct platform_device *pdev)
{
	struct lp8x4x_master *m, **p;
	struct resource *res;
	int r = 0;
	int i;
	int err = 0;
	unsigned int model;

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

	for (i = 0; i < LP8X4X_MAX_SLOT_COUNT; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, r++);
		if (!res) {
			dev_err(&pdev->dev, "Failed to get slot %i address\n",
					i);
			err = -ENODEV;
			goto err_free;
		}

		m->slot[i].data_addr = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(m->slot[i].data_addr)) {
			dev_err(&pdev->dev, "Failed to ioremap slot %i\n", i);
			err = PTR_ERR(m->slot[i].data_addr);
			goto err_free;
		}
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
		dev_err(&pdev->dev, "Failed to get slot switch address\n");
		err = -ENODEV;
		goto err_free;
	}

	m->switch_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(m->switch_addr)) {
		dev_err(&pdev->dev, "Failed to ioremap switch address\n");
		err = PTR_ERR(m->switch_addr);
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

	/* Disable serial communications until explicitly enabled */
	iowrite8(0xff, m->switch_addr);

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
	for (i = 0; i < LP8X4X_MAX_SLOT_COUNT; i++) {
		model = lp8x4x_model[ioread8(m->slot[i].data_addr)];
		if (!model)
			continue;

		lp8x4x_bus_probe_slot(m, i, model);
	}
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
