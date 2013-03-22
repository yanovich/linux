/*
 * linux/drivers/w1/notify.c
 *
 * Notification logic for 1-wire subsystem
 *
 * (C) Copyright 2013 Sergey Yanovich
 *
 * This is mostly a copy with s/usb/w1/gc from
 * linux/drivers/usb/notify.c
 *
 * All the USB notify logic
 *
 * (C) Copyright 2005 Greg Kroah-Hartman <gregkh@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef CONFIG_W1_NOTIFY

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/notifier.h>
#include <linux/w1.h>
#include <linux/mutex.h>
#include "w1.h"

static BLOCKING_NOTIFIER_HEAD(w1_notifier_list);

/**
 * w1_register_notify - register a notifier callback whenever a w1 change happens
 * @nb: pointer to the notifier block for the callback events.
 *
 * These changes are either w1 devices or busses being added or removed.
 */
void w1_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&w1_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(w1_register_notify);

/**
 * w1_unregister_notify - unregister a notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * w1_register_notify() must have been previously called for this function
 * to work properly.
 */
void w1_unregister_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&w1_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(w1_unregister_notify);

void w1_notify_add_slave(struct w1_slave *dev)
{
	blocking_notifier_call_chain(&w1_notifier_list, W1_SLAVE_ADD, dev);
}

void w1_notify_remove_slave(struct w1_slave *dev)
{
	blocking_notifier_call_chain(&w1_notifier_list, W1_SLAVE_REMOVE, dev);
}

void w1_notify_add_master(struct w1_master *bus)
{
	blocking_notifier_call_chain(&w1_notifier_list, W1_MASTER_ADD, bus);
}

void w1_notify_remove_master(struct w1_master *bus)
{
	blocking_notifier_call_chain(&w1_notifier_list, W1_MASTER_REMOVE, bus);
}
#endif
