/*
 *  arch/arm/mach-pxa/include/mach/lp8x4x.h
 *
 *  Support for ICP DAS LP-8x4x programmable automation controller
 *  Copyright (C) 2013 Sergey Yanovich
 *
 *  borrowed heavily from
 *  Support for the Intel HCDDBBVA0 Development Platform.
 *
 *  Author:	Nicolas Pitre
 *  Created:	Nov 14, 2002
 *  Copyright:	MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef ASM_ARCH_LP8X4X_H
#define ASM_ARCH_LP8X4X_H

#include <mach/irqs.h>

#define LP8X4X_ETH0_BASE        (0x0c000000)
#define LP8X4X_ETH0_IO          (0x0c004000)
#define LP8X4X_ETH0_IRQ         PXA_GPIO_TO_IRQ(9)

#define LP8X4X_ETH1_BASE        (0x0d000000)
#define LP8X4X_ETH1_IO          (0x0d004000)
#define LP8X4X_ETH1_IRQ         PXA_GPIO_TO_IRQ(82)

#define LP8X4X_FPGA_PHYS	(0x17000000)
#define LP8X4X_FPGA_VIRT	(0xf1000000)
#define LP8X4X_P2V(x)		((x) - LP8X4X_FPGA_PHYS + LP8X4X_FPGA_VIRT)
#define LP8X4X_V2P(x)		((x) - LP8X4X_FPGA_VIRT + LP8X4X_FPGA_PHYS)

#ifndef __ASSEMBLY__
# define __LP8X4X_REG(x)	(*((volatile unsigned long *)LP8X4X_P2V(x)))
# define __LP8X4X_MEM(x)	(*((volatile unsigned long *)(x)))
#else
# define __LP8X4X_REG(x)	LP8X4X_P2V(x)
# define __LP8X4X_MEM(x)	(x)
#endif

/* board level registers in the FPGA */

#define LP8X4X_RWRTC		__LP8X4X_REG(0x1700901c)
#define LP8X4X_MOD_NUM		__LP8X4X_REG(0x17009046)

/* board specific IRQs */

#define LP8X4X_IRQ(x)		(IRQ_BOARD_START + (x))
#define LP8X4X_SLOT1_IRQ	LP8X4X_IRQ(0)
#define LP8X4X_SLOT2_IRQ	LP8X4X_IRQ(1)
#define LP8X4X_SLOT3_IRQ	LP8X4X_IRQ(2)
#define LP8X4X_SLOT4_IRQ	LP8X4X_IRQ(3)
#define LP8X4X_SLOT5_IRQ	LP8X4X_IRQ(4)
#define LP8X4X_SLOT6_IRQ	LP8X4X_IRQ(5)
#define LP8X4X_SLOT7_IRQ	LP8X4X_IRQ(6)
#define LP8X4X_SLOT8_IRQ	LP8X4X_IRQ(7)
#define LP8X4X_TIMER1_IRQ	LP8X4X_IRQ(8)
#define LP8X4X_TIMER2_IRQ	LP8X4X_IRQ(9)
#define LP8X4X_TIMEROUT_IRQ	LP8X4X_IRQ(10)
#define LP8X4X_HOTPLUG_IRQ	LP8X4X_IRQ(11)
#define LP8X4X_BATLOW_IRQ	LP8X4X_IRQ(12)
#define LP8X4X_COM2_IRQ	 	LP8X4X_IRQ(13)
#define LP8X4X_COM3_IRQ		LP8X4X_IRQ(14)
#define LP8X4X_COM4_IRQ		LP8X4X_IRQ(15)

#define LP8X4X_NR_IRQS		(IRQ_BOARD_START + 16)

extern void nsleep (unsigned long nanosec);

#endif
