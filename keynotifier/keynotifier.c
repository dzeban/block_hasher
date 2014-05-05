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

#include "keynotifier.h"

// Keyboard notification callback
static int kb_nf_callback(struct notifier_block *nb, unsigned long code, void *_param)
{
	struct keyboard_notifier_param *param = NULL;

	param = (struct keyboard_notifier_param *)_param;

	if (!param)
	{
		pr_err("Bad keyboard notification\n");
		return NOTIFY_BAD;
	}

	if (param->down && code == KBD_KEYCODE)
        pr_info("Key: %d\n", param->value);

	return NOTIFY_DONE;
}

static void cleanup(void)
{
	int rc = 0;
	
	rc = unregister_keyboard_notifier(&kb_nf);
	if (rc)
	{
		pr_err("Failed to unregister keyboard notifier. Error %d\n", rc);
	}

	return;
}

static int keynotifier_init(void)
{
	int rc = 0;

	// ---------------------------------------------
	// Register keyboard notifier
	// ---------------------------------------------
	rc = register_keyboard_notifier(&kb_nf);
	if (rc)
	{
		pr_err("Failed to register keyboard notifier. Error %d\n", rc);
        cleanup();
        return -EFAULT;
	}

	return 0;
}

static void keynotifier_exit(void)
{
	cleanup();
	return;
}

module_init(keynotifier_init);
module_exit(keynotifier_exit);

MODULE_AUTHOR("Alex Dzyoba <avd@reduct.ru>");
MODULE_DESCRIPTION("Keyboard press notifier");
MODULE_LICENSE("GPL");
