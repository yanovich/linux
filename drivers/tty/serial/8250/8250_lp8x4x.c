/*  linux/drivers/tty/serial/8250/8250_lp8x4x.c
 *
 *  Support for 16550A serial ports on ICP DAS LP-8x4x
 *
 *  Copyright (C) 2013 Sergei Ianovich <ynvich@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/serial_8250.h>
#include <linux/of.h>
#include <linux/platform_device.h>

struct lp8x4x_serial_data {
	int			line;
	void			*ios_mem;
};

static void lp8x4x_serial_set_termios(struct uart_port *port,
		struct ktermios *termios, struct ktermios *old)
{
	unsigned int len;
	unsigned int baud;
	struct lp8x4x_serial_data *data = port->private_data;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		len = 7;
		break;
	case CS6:
		len = 8;
		break;
	case CS7:
		len = 9;
		break;
	default:
	case CS8:
		len = 10;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		len++;
	if (termios->c_cflag & PARENB)
		len++;
	if (!(termios->c_cflag & PARODD))
		len++;
#ifdef CMSPAR
	if (termios->c_cflag & CMSPAR)
		len++;
#endif

	len -= 9;
	len &= 3;
	len <<= 3;
	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old,
				  port->uartclk / 16 / 0xffff,
				  port->uartclk / 16);
	switch (baud) {
	case 2400:
		len |= 1;
	case 4800:
		len |= 2;
	case 19200:
		len |= 4;
	case 38400:
		len |= 5;
	case 57600:
		len |= 6;
	case 115200:
		len |= 7;
	case 9600:
	default:
		len |= 3;
	};
	iowrite8(len, data->ios_mem);

	serial8250_do_set_termios(port, termios, old);
}

static struct of_device_id lp8x4x_serial_dt_ids[] = {
	{ .compatible = "icpdas,uart-lp8x4x", },
	{}
};
MODULE_DEVICE_TABLE(of, lp8x4x_serial_dt_ids);

static int lp8x4x_serial_probe(struct platform_device *pdev)
{
	struct uart_8250_port uart = {};
	struct lp8x4x_serial_data *data;
	struct resource *mmres, *mires, *irqres;
	int ret;

	mmres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mires = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	irqres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!mmres || !mires || !irqres)
		return -ENODEV;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->ios_mem = devm_ioremap_resource(&pdev->dev, mires);
	if (!data->ios_mem)
		return -EFAULT;

	uart.port.iotype = UPIO_MEM;
	uart.port.mapbase = mmres->start;
	uart.port.iobase = mmres->start;
	uart.port.regshift = 1;
	uart.port.irq = irqres->start;
	uart.port.flags = UPF_IOREMAP;
	uart.port.dev = &pdev->dev;
	uart.port.uartclk = 14745600;
	uart.port.set_termios = lp8x4x_serial_set_termios;
	uart.port.private_data = data;

	ret = serial8250_register_8250_port(&uart);
	if (ret < 0)
		return ret;

	data->line = ret;

	platform_set_drvdata(pdev, data);

	return 0;
}

static int lp8x4x_serial_remove(struct platform_device *pdev)
{
	struct lp8x4x_serial_data *data = platform_get_drvdata(pdev);

	serial8250_unregister_port(data->line);

	return 0;
}

static struct platform_driver lp8x4x_serial_driver = {
	.probe          = lp8x4x_serial_probe,
	.remove         = lp8x4x_serial_remove,

	.driver		= {
		.name	= "uart-lp8x4x",
		.owner	= THIS_MODULE,
		.of_match_table = lp8x4x_serial_dt_ids,
	},
};

module_platform_driver(lp8x4x_serial_driver);

MODULE_AUTHOR("Sergei Ianovich");
MODULE_DESCRIPTION("8250 serial port module for LP-8x4x");
MODULE_LICENSE("GPL");
