/* 8250_lp8x4x.c -- probe 8250 serial on ICP DAS LP-8x4x
 *  Copyright (C) 2013 Sergey Yanovich
 *  Data taken from drivers/tty/serial/8250/8250_accent.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/serial_8250.h>
#include <mach/lp8x4x.h>
#include <asm/io.h>

#define PORT(_base,_irq,cb)					\
	{							\
		.iobase		= _base,			\
		.membase	= (void *) _base,		\
		.mapbase	= _base,			\
		.irq		= _irq,				\
		.private_data	= (void *) &__LP8X4X_REG(cb),	\
		.uartclk	= 14745600,			\
		.regshift	= 1,				\
		.iotype		= UPIO_MEM,			\
		.flags		= UPF_IOREMAP,			\
		.set_termios	= lp8x4x_set_termios,		\
		.serial_in	= lp8x4x_serial_in,		\
		.serial_out	= lp8x4x_serial_out,		\
	}

static void lp8x4x_set_termios(struct uart_port *port,
	       	struct ktermios *termios, struct ktermios *old)
{
	unsigned int len;
	unsigned int baud;

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
	*((volatile unsigned char *)port->private_data) = (unsigned char) len;

	serial8250_do_set_termios(port, termios, old);
}

static unsigned int lp8x4x_serial_in(struct uart_port *p, int offset)
{
	unsigned int b;
	udelay(30);
	offset = offset << p->regshift;
	b = readb(p->membase + offset);
	return b;
}

static void lp8x4x_serial_out(struct uart_port *p, int offset, int value)
{
	offset = offset << p->regshift;
	writeb(value, p->membase + offset);
}

static struct plat_serial8250_port lp8x4x_data[] = {
	PORT(0x17009050, LP8X4X_TTYS0_IRQ, 0x17009030),
	PORT(0x17009060, LP8X4X_TTYS1_IRQ, 0x17009032),
	{ },
};

static struct platform_device lp8x4x_device = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_ACCENT,
	.dev			= {
		.platform_data	= lp8x4x_data,
	},
};

static int __init lp8x4x_init(void)
{
	return platform_device_register(&lp8x4x_device);
}

module_init(lp8x4x_init);

MODULE_AUTHOR("Sergey Yanovich");
MODULE_DESCRIPTION("8250 serial probe module for LP-8x4x");
MODULE_LICENSE("GPL");
