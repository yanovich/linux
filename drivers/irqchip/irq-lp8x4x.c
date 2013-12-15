/*
 *  linux/drivers/irqchip/irq-lp8x4x.c
 *
 *  Support for ICP DAS LP-8x4x FPGA irq
 *  Copyright (C) 2013 Sergei Ianovich <ynvich@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation or any later version.
 */
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define EOI			0x00000000
#define INSINT			0x00000002
#define ENSYSINT		0x00000004
#define PRIMINT			0x00000006
#define SECOINT			0x00000008
#define ENRISEINT		0x0000000A
#define CLRRISEINT		0x0000000C
#define ENHILVINT		0x0000000E
#define CLRHILVINT		0x00000010
#define ENFALLINT		0x00000012
#define CLRFALLINT		0x00000014
#define LP8X4X_IRQ_MEM_SIZE	0x00000016

static unsigned char irq_sys_enabled;
static unsigned char irq_high_enabled;
static void *base;
static int irq_base;
static int num_irq = 16;

static void lp8x4x_mask_irq(struct irq_data *d)
{
	unsigned mask;
	int irq = d->irq - irq_base;

	if (irq < 0 || irq > 15) {
		pr_err("lp8x4x: wrong irq handler for irq %i\n", d->irq);
		return;
	}

	if (irq < 8) {
		irq_high_enabled &= ~(1 << irq);

		mask = ioread8(base + ENHILVINT);
		mask &= ~(1 << irq);
		iowrite8(mask, base + ENHILVINT);
	} else {
		irq -= 8;
		irq_sys_enabled &= ~(1 << irq);

		mask = ioread8(base + ENSYSINT);
		mask &= ~(1 << irq);
		iowrite8(mask, base + ENSYSINT);
	}
}

static void lp8x4x_unmask_irq(struct irq_data *d)
{
	unsigned mask;
	int irq = d->irq - irq_base;

	if (irq < 0 || irq > 15) {
		pr_err("wrong irq handler for irq %i\n", d->irq);
		return;
	}

	if (irq < 8) {
		irq_high_enabled |= 1 << irq;
		mask = ioread8(base + CLRHILVINT);
		mask |= 1 << irq;
		iowrite8(mask, base + CLRHILVINT);

		mask = ioread8(base + ENHILVINT);
		mask |= 1 << irq;
		iowrite8(mask, base + ENHILVINT);
	} else {
		irq -= 8;
		irq_sys_enabled |= 1 << irq;

		mask = ioread8(base + SECOINT);
		mask |= 1 << irq;
		iowrite8(mask, base + SECOINT);

		mask = ioread8(base + ENSYSINT);
		mask |= 1 << irq;
		iowrite8(mask, base + ENSYSINT);
	}
}

static struct irq_chip lp8x4x_irq_chip = {
	.name			= "FPGA",
	.irq_ack		= lp8x4x_mask_irq,
	.irq_mask		= lp8x4x_mask_irq,
	.irq_mask_ack		= lp8x4x_mask_irq,
	.irq_unmask		= lp8x4x_unmask_irq,
};

static void lp8x4x_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	int loop, n;
	unsigned long mask;
	struct irq_chip *chip = irq_desc_get_chip(desc);

	chained_irq_enter(chip, desc);

	do {
		loop = 0;
		mask = ioread8(base + CLRHILVINT) & 0xff;
		mask |= (ioread8(base + SECOINT) & 0x1f) << 8;
		mask |= (ioread8(base + PRIMINT) & 0xe0) << 8;
		mask &= (irq_high_enabled | (irq_sys_enabled << 8));
		for_each_set_bit(n, &mask, BITS_PER_LONG) {
			loop = 1;

			generic_handle_irq(irq_base + n);
		}
	} while (loop);

	iowrite8(0, base + EOI);
	chained_irq_exit(chip, desc);
}

static int lp8x4x_irq_domain_map(struct irq_domain *d, unsigned int irq,
				 irq_hw_number_t hw)
{
	irq_set_chip_and_handler(irq, &lp8x4x_irq_chip,
				 handle_level_irq);
	set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	return 0;
}

const struct irq_domain_ops lp8x4x_irq_domain_ops = {
	.map	= lp8x4x_irq_domain_map,
	.xlate	= irq_domain_xlate_onecell,
};

static struct of_device_id lp8x4x_irq_dt_ids[] = {
	{ .compatible = "icpdas,irq-lp8x4x", },
	{}
};

static int lp8x4x_irq_probe(struct platform_device *pdev)
{
	struct resource *rm, *ri;
	struct device_node *np = pdev->dev.of_node;
	struct irq_domain *domain;

	rm = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ri = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!rm || !ri || resource_size(rm) < LP8X4X_IRQ_MEM_SIZE)
		return -ENODEV;

	base = devm_ioremap_resource(&pdev->dev, rm);
	if (!base) {
		dev_err(&pdev->dev, "Failed to ioremap %p\n", base);
		return -EFAULT;
	}

	irq_base = irq_alloc_descs(-1, 0, num_irq, 0);
	if (irq_base < 0) {
		dev_err(&pdev->dev, "Failed to allocate IRQ numbers\n");
		return irq_base;
	}

	domain = irq_domain_add_legacy(np, num_irq, irq_base, 0,
				       &lp8x4x_irq_domain_ops, NULL);
	if (!domain) {
		dev_err(&pdev->dev, "Failed to add IRQ domain\n");
		return -ENOMEM;
	}

	iowrite8(0, base + CLRRISEINT);
	iowrite8(0, base + ENRISEINT);
	iowrite8(0, base + CLRFALLINT);
	iowrite8(0, base + ENFALLINT);
	iowrite8(0, base + CLRHILVINT);
	iowrite8(0, base + ENHILVINT);
	iowrite8(0, base + ENSYSINT);
	iowrite8(0, base + SECOINT);

	irq_set_chained_handler(ri->start, lp8x4x_irq_handler);

	return 0;
}

static struct platform_driver lp8x4x_irq_driver = {
	.probe		= lp8x4x_irq_probe,
	.driver		= {
		.name	= "irq-lp8x4x",
		.of_match_table = lp8x4x_irq_dt_ids,
	},
};

static int __init lp8x4x_irq_init(void)
{
	return platform_driver_register(&lp8x4x_irq_driver);
}
postcore_initcall(lp8x4x_irq_init);
