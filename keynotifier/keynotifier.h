/*
 * Keynotifier - keyboard press notifier
 *
 * Copyright (C) 2014  Alex Dzyoba <avd@reduct.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the init macros */

#include <linux/keyboard.h> /* Needed for                   */
#include <linux/notifier.h> /*            keyboard notifier */

#include <linux/hardirq.h>

// -----------------------------------------------------------------------------
// Keyboard notifier
// -----------------------------------------------------------------------------
static int kb_nf_callback(struct notifier_block *nb, unsigned long code, void *_param);
static struct notifier_block kb_nf = {
	.notifier_call = kb_nf_callback
};
