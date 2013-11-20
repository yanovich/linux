/*
 *  linux/arch/arm/mach-pxa/lp8x4x.c
 *
 *  Support for ICP DAS LP-8x4x programmable automation controller
 *  Copyright (C) 2013 Sergey Yanovich
 *
 *  borrowed heavily from
 *  Support for the Intel HCDDBBVA0 Development Platform.
 *
 *  Author:	Nicolas Pitre
 *  Created:	Nov 05, 2002
 *  Copyright:	MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation or any later version.
 */
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/syscore_ops.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/bitops.h>
#include <linux/fb.h>
#include <linux/ioport.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/input.h>
#include <linux/dm9000.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/ktime.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/sizes.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/mach/flash.h>

#include <mach/pxa27x.h>
#include <mach/mainstone.h>
#include <mach/lp8x4x.h>
#include <linux/platform_data/video-pxafb.h>
#include <linux/platform_data/mmc-pxamci.h>
#include <linux/platform_data/usb-ohci-pxa27x.h>
#include <mach/smemc.h>

#include "generic.h"
#include "devices.h"

static unsigned char lp8x4x_irq_sys_enabled = 0;
static unsigned char lp8x4x_irq_high_enabled = 0;

static void lp8x4x_ack_irq(struct irq_data *d)
{
	int irq = d->irq - IRQ_BOARD_START;
	if (irq < 0 || irq > 15) {
		printk(KERN_ERR "wrong irq handler for irq %i\n", d->irq);
		return;
	}

	if (irq < 8) {
		LP8X4X_CLRHILVINT |= 1 << irq;
	} else if (irq < 13) {
		irq -= 8;
		LP8X4X_SECOINT |= 1 << irq;
	} else {
		irq -= 8;
		LP8X4X_PRIMINT |= 1 << irq;
	}
}

static void lp8x4x_mask_irq(struct irq_data *d)
{
	int irq = d->irq - IRQ_BOARD_START;
	if (irq < 0 || irq > 15) {
		printk(KERN_ERR "wrong irq handler for irq %i\n", d->irq);
		return;
	}

	if (irq < 8) {
		lp8x4x_irq_high_enabled &= ~(1 << irq);
		LP8X4X_ENHILVINT = lp8x4x_irq_high_enabled;
	} else {
		irq -= 8;
		lp8x4x_irq_sys_enabled &= ~(1 << irq);
		LP8X4X_ENSYSINT = lp8x4x_irq_sys_enabled;
	}
}

static void lp8x4x_unmask_irq(struct irq_data *d)
{
	int irq = d->irq - IRQ_BOARD_START;
	if (irq < 0 || irq > 15) {
		printk(KERN_ERR "wrong irq handler for irq %i\n", d->irq);
		return;
	}

	if (irq < 8) {
		lp8x4x_irq_high_enabled |= 1 << irq;
		LP8X4X_CLRHILVINT |= 1 << irq;
		LP8X4X_ENHILVINT = lp8x4x_irq_high_enabled;
	} else if (irq < 13) {
		irq -= 8;
		lp8x4x_irq_sys_enabled |= 1 << irq;
		LP8X4X_SECOINT |= 1 << irq;
		LP8X4X_ENSYSINT |= 1 << irq;
	} else {
		irq -= 8;
		lp8x4x_irq_sys_enabled |= 1 << irq;
		LP8X4X_SECOINT |= 1 << irq;
		LP8X4X_PRIMINT |= 1 << irq;
		LP8X4X_ENSYSINT |= 1 << irq;
	}
}

static struct irq_chip lp8x4x_irq_chip = {
	.name			= "FPGA",
	.irq_ack		= lp8x4x_ack_irq,
	.irq_mask		= lp8x4x_mask_irq,
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
		mask = LP8X4X_CLRHILVINT & 0xff;
		mask |= ((LP8X4X_SECOINT & 0x1f) << 8);
		mask |= ((LP8X4X_PRIMINT & 0xe0) << 8);
		mask &= (lp8x4x_irq_high_enabled
				| (lp8x4x_irq_sys_enabled << 8));
		for_each_set_bit(n, &mask, BITS_PER_LONG) {
			loop = 1;

			generic_handle_irq(IRQ_BOARD_START + n);
		}
	} while (loop);

	chained_irq_exit(chip, desc);
	LP8X4X_EOI = 0x0;
}

static void __init lp8x4x_init_irq(void)
{
	int irq;

	for(irq = IRQ_BOARD_START; irq < LP8X4X_NR_IRQS; irq++) {
		irq_set_chip_and_handler(irq, &lp8x4x_irq_chip,
					 handle_level_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	irq_set_chained_handler(PXA_GPIO_TO_IRQ(3), lp8x4x_irq_handler);
	irq_set_irq_type(PXA_GPIO_TO_IRQ(3), IRQ_TYPE_EDGE_RISING);

	LP8X4X_CLRRISEINT = 0;
	LP8X4X_ENRISEINT = 0;
	LP8X4X_CLRFALLINT = 0;
	LP8X4X_ENFALLINT = 0;
	LP8X4X_CLRHILVINT = 0;
	LP8X4X_ENHILVINT = 0;
	LP8X4X_ENSYSINT = 0;
	LP8X4X_PRIMINT = 0;
	LP8X4X_SECOINT = 0;

	return;
}

static unsigned long lp8x4x_pin_config[] = {
	/* MMC */
	GPIO32_MMC_CLK,
	GPIO112_MMC_CMD,
	GPIO92_MMC_DAT_0,
	GPIO109_MMC_DAT_1,
	GPIO110_MMC_DAT_2,
	GPIO111_MMC_DAT_3,

	/* USB Host Port 1 */
	GPIO88_USBH1_PWR,
	GPIO89_USBH1_PEN,
};

static struct resource lp8x4x_flash_resources[] = {
	[0] = {
		.start	= PXA_CS0_PHYS,
		.end	= PXA_CS0_PHYS + SZ_64M - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= PXA_CS1_PHYS,
		.end	= PXA_CS1_PHYS + SZ_32M - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct mtd_partition lp8x4x_flash0_partitions[] = {
	{
		.name =		"Bootloader",
		.size =		0x00040000,
		.offset =	0,
	},{
		.name =		"Settings",
		.size =		0x00040000,
		.offset =	0x00040000,
	},{
		.name =		"Kernel",
		.size =		0x00280000,
		.offset =	0x00080000,
	},{
		.name =		"Filesystem",
		.size =		MTDPART_SIZ_FULL,
		.offset =	0x00300000
	}
};

static struct flash_platform_data lp8x4x_flash_data[2] = {
	{
		.map_name	= "cfi_probe",
		.parts		= lp8x4x_flash0_partitions,
		.nr_parts	= ARRAY_SIZE(lp8x4x_flash0_partitions),
		.width		= 4,
	}, {
		.map_name	= "cfi_probe",
		.parts		= NULL,
		.nr_parts	= 0,
		.width		= 2,
	}
};

static struct platform_device lp8x4x_flash_device[2] = {
	{
		.name		= "pxa2xx-flash",
		.id		= 0,
		.dev = {
			.platform_data = &lp8x4x_flash_data[0],
		},
		.resource = &lp8x4x_flash_resources[0],
		.num_resources = 1,
	},
	{
		.name		= "pxa2xx-flash",
		.id		= 1,
		.dev = {
			.platform_data = &lp8x4x_flash_data[1],
		},
		.resource = &lp8x4x_flash_resources[1],
		.num_resources = 1,
	},
};

static struct pxafb_mode_info lp8x4x_vga_60_mode = {
	.pixclock       = 38461,
	.xres           = 640,
	.yres           = 480,
	.bpp            = 16,
	.hsync_len      = 64,
	.left_margin    = 78,
	.right_margin   = 46,
	.vsync_len      = 12,
	.upper_margin   = 22,
	.lower_margin   = 10,
	.sync           = 0,
};

static struct pxafb_mach_info lp8x4x_pxafb_info = {
	.num_modes      	= 1,
	.lccr0			= LCCR0_Act,
	.lccr3			= LCCR3_PCP,
};

static int lp8x4x_mci_init(struct device *dev,
	       	irq_handler_t mstone_detect_int, void *data)
{
	/* make sure SD/Memory Stick multiplexer's signals
	 * are routed to MMC controller
	 */
	MST_MSCWR1 &= ~MST_MSCWR1_MS_SEL;

	return 0;
}

static void lp8x4x_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data* p_d = dev->platform_data;

	if (( 1 << vdd) & p_d->ocr_mask) {
		printk(KERN_DEBUG "%s: on\n", __func__);
		MST_MSCWR1 |= MST_MSCWR1_MMC_ON;
		MST_MSCWR1 &= ~MST_MSCWR1_MS_SEL;
	} else {
		printk(KERN_DEBUG "%s: off\n", __func__);
		MST_MSCWR1 &= ~MST_MSCWR1_MMC_ON;
	}
}

static void lp8x4x_mci_exit(struct device *dev, void *data)
{
}

static struct pxamci_platform_data lp8x4x_mci_platform_data = {
	.ocr_mask		= MMC_VDD_32_33|MMC_VDD_33_34,
	.init			= lp8x4x_mci_init,
	.setpower               = lp8x4x_mci_setpower,
	.exit			= lp8x4x_mci_exit,
	.gpio_card_detect	= -1,
	.gpio_card_ro		= -1,
	.gpio_power		= -1,
};

static struct resource lp8x4x_dm9000_resources[] = {
	[0] = {
		.start  = LP8X4X_ETH0_BASE,
		.end    = LP8X4X_ETH0_BASE + 2 - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = LP8X4X_ETH0_IO,
		.end    = LP8X4X_ETH0_IO + 2 - 1,
		.flags  = IORESOURCE_MEM,
	},      
	[2] = { 
		.start  = LP8X4X_ETH0_IRQ,
		.end    = LP8X4X_ETH0_IRQ,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},      
	[3] = { 
		.start  = LP8X4X_ETH1_BASE,
		.end    = LP8X4X_ETH1_BASE + 2 - 1,
		.flags  = IORESOURCE_MEM,
	},      
	[4] = { 
		.start  = LP8X4X_ETH1_IO,
		.end    = LP8X4X_ETH1_IO + 2 - 1,
		.flags  = IORESOURCE_MEM,
	},
	[5] = {
		.start  = LP8X4X_ETH1_IRQ,
		.end    = LP8X4X_ETH1_IRQ,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct platform_device lp8x4x_dm9000_device[2] = {
	{
		.name           = "dm9000",
		.id             = 0,
		.dev = {},
		.resource = &lp8x4x_dm9000_resources[0],
		.num_resources = 3,
	},
	{
		.name           = "dm9000",
		.id             = 1,
		.dev = {},
		.resource = &lp8x4x_dm9000_resources[3],
		.num_resources = 3,
	},
};      

static struct platform_device lp8x4x_ds1302_device = {
	.name           = "rtc-ds1302",
	.id             = 0,
};      

static struct platform_device *lp8x4x_devices[] __initdata = {
	&lp8x4x_flash_device[0],
	&lp8x4x_flash_device[1],
	&lp8x4x_dm9000_device[0],
	&lp8x4x_dm9000_device[1],
	&lp8x4x_ds1302_device,
};

static struct pxaohci_platform_data lp8x4x_ohci_platform_data = {
	.port_mode	= PMM_PERPORT_MODE,
	.flags		= ENABLE_PORT1 | OC_MODE_PERPORT,
};

static void __init lp8x4x_init(void)
{
	pxa2xx_mfp_config(ARRAY_AND_SIZE(lp8x4x_pin_config));

	pxa_set_ffuart_info(NULL);
	pxa_set_btuart_info(NULL);
	pxa_set_stuart_info(NULL);

	/* system bus arbiter setting
	 * - Core_Park
	 * - LCD_wt:DMA_wt:CORE_Wt = 2:3:4
	 */
	ARB_CNTRL = ARB_CORE_PARK | 0x234;

	platform_add_devices(ARRAY_AND_SIZE(lp8x4x_devices));

	lp8x4x_pxafb_info.modes = &lp8x4x_vga_60_mode;
	pxa_set_fb_info(NULL, &lp8x4x_pxafb_info);

	pxa_set_mci_info(&lp8x4x_mci_platform_data);
	pxa_set_ohci_info(&lp8x4x_ohci_platform_data);

	/* Could not do this in MACHINE since GPIO is not ready then */
	lp8x4x_init_irq();
}

static struct map_desc lp8x4x_io_desc[] __initdata = {
  	{	/* CPLD */
		.virtual	=  MST_FPGA_VIRT,
		.pfn		= __phys_to_pfn(MST_FPGA_PHYS),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	}
  	,{	/* CPLD */
		.virtual	=  LP8X4X_FPGA_VIRT,
		.pfn		= __phys_to_pfn(LP8X4X_FPGA_PHYS),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	}
};

static void __init lp8x4x_map_io(void)
{
	pxa27x_map_io();
	iotable_init(lp8x4x_io_desc, ARRAY_SIZE(lp8x4x_io_desc));
}

/*
 * Driver for the 8 discrete LEDs available for general use:
 * Note: bits [15-8] are used to enable/blank the 8 7 segment hex displays
 * so be sure to not monkey with them here.
 */

#if defined(CONFIG_NEW_LEDS) && defined(CONFIG_LEDS_CLASS)
struct mainstone_led {
	struct led_classdev	cdev;
	u8			mask;
};

/*
 * The triggers lines up below will only be used if the
 * LED triggers are compiled in.
 */
static const struct {
	const char *name;
	const char *trigger;
} mainstone_leds[] = {
	{ "mainstone:D28", "default-on", },
	{ "mainstone:D27", "cpu0", },
	{ "mainstone:D26", "heartbeat" },
	{ "mainstone:D25", },
	{ "mainstone:D24", },
	{ "mainstone:D23", },
	{ "mainstone:D22", },
	{ "mainstone:D21", },
};

static void mainstone_led_set(struct led_classdev *cdev,
			      enum led_brightness b)
{
	struct mainstone_led *led = container_of(cdev,
					 struct mainstone_led, cdev);
	u32 reg = MST_LEDCTRL;

	if (b != LED_OFF)
		reg |= led->mask;
	else
		reg &= ~led->mask;

	MST_LEDCTRL = reg;
}

static enum led_brightness mainstone_led_get(struct led_classdev *cdev)
{
	struct mainstone_led *led = container_of(cdev,
					 struct mainstone_led, cdev);
	u32 reg = MST_LEDCTRL;

	return (reg & led->mask) ? LED_FULL : LED_OFF;
}

static int __init lp8x4x_leds_init(void)
{
	int i;

	if (!machine_is_lp8x4x())
		return -ENODEV;

	/* All ON */
	MST_LEDCTRL |= 0xff;
	for (i = 0; i < ARRAY_SIZE(mainstone_leds); i++) {
		struct mainstone_led *led;

		led = kzalloc(sizeof(*led), GFP_KERNEL);
		if (!led)
			break;

		led->cdev.name = mainstone_leds[i].name;
		led->cdev.brightness_set = mainstone_led_set;
		led->cdev.brightness_get = mainstone_led_get;
		led->cdev.default_trigger = mainstone_leds[i].trigger;
		led->mask = BIT(i);

		if (led_classdev_register(NULL, &led->cdev) < 0) {
			kfree(led);
			break;
		}
	}

	return 0;
}

/*
 * Since we may have triggers on any subsystem, defer registration
 * until after subsystem_init.
 */
fs_initcall(lp8x4x_leds_init);
#endif

void nsleep (unsigned long nanosec)
{
	ktime_t t = ns_to_ktime (nanosec);
	long state = current->state;

	__set_current_state (TASK_UNINTERRUPTIBLE);
	schedule_hrtimeout (&t, HRTIMER_MODE_REL);
	__set_current_state (state);
}
EXPORT_SYMBOL_GPL(nsleep);

MACHINE_START(LP8X4X, "ICP DAS LP-8x4x programmable automation controller")
	/* Maintainer: MontaVista Software Inc. */
	.atag_offset	= 0x100,	/* BLOB boot parameter setting */
	.map_io		= lp8x4x_map_io,
	.nr_irqs	= LP8X4X_NR_IRQS,
	.init_irq	= pxa27x_init_irq,
	.handle_irq	= pxa27x_handle_irq,
	.timer		= &pxa_timer,
	.init_machine	= lp8x4x_init,
	.restart	= pxa_restart,
MACHINE_END
