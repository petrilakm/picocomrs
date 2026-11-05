/* vi: set sw=4 ts=4:
 *
 * te_gpio.h
 *
 * Drive a GPIO line as the transmit-enable (TE / DE) signal of an
 * RS485 transceiver. Uses the Linux GPIO character-device interface
 * (/dev/gpiochipN); no external libraries are required.
 *
 * Built only when USE_TE_GPIO is defined (the Makefile defines it
 * automatically when building on a Raspberry Pi).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef TE_GPIO_H
#define TE_GPIO_H

/* F te_gpio_open
 *
 * Request the GPIO line given by "spec" as an output, and set it to
 * the inactive (receive) state. "spec" is "[<chip>:]<line>", where
 * <chip> is either a device path ("/dev/gpiochip4") or a device name
 * ("gpiochip4"), and <line> is the line offset on that chip (on a
 * Raspberry Pi, the BCM GPIO number). If <chip> is omitted, the chip
 * driving the Raspberry Pi header is located automatically (falling
 * back to /dev/gpiochip0). If "active_low" is non-zero, the line is
 * driven low while transmitting, else it is driven high.
 *
 * Returns 0 on success, or a negative value on failure, in which case
 * errno is set and te_gpio_strerror() describes the failure.
 */
int te_gpio_open (const char *spec, int active_low);

/* F te_gpio_set
 *
 * Set the TE line active (on != 0, transmit) or inactive (on == 0,
 * receive). Does nothing if the line is not open, or if it is already
 * in the requested state. Returns 0 on success, or a negative value
 * (with errno set) on failure.
 */
int te_gpio_set (int on);

/* F te_gpio_is_active
 *
 * Returns non-zero if the TE line is open and currently active.
 */
int te_gpio_is_active (void);

/* F te_gpio_close
 *
 * Set the TE line inactive and release it. Safe to call if the line
 * was never opened.
 */
void te_gpio_close (void);

/* F te_gpio_desc
 *
 * Returns a string describing the open line (e.g. "/dev/gpiochip0:17"),
 * or NULL if no line is open.
 */
const char *te_gpio_desc (void);

/* F te_gpio_strerror
 *
 * Returns a string describing why the last te_gpio_open() failed.
 */
const char *te_gpio_strerror (void);

#endif /* of TE_GPIO_H */

/**********************************************************************/

/*
 * Local Variables:
 * mode:c
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
