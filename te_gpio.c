/* vi: set sw=4 ts=4:
 *
 * te_gpio.c
 *
 * Drive a GPIO line as the transmit-enable (TE / DE) signal of an
 * RS485 transceiver. See te_gpio.h.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

#include "te_gpio.h"

#define TE_CONSUMER "picocom-te"

/* Labels of the gpiochips that drive the 40-pin header on the
   various Raspberry Pi models. */
static const char *rpi_chip_labels[] = {
    "pinctrl-bcm2835",   /* Pi 1, 2, 3, Zero */
    "pinctrl-bcm2711",   /* Pi 4, 400, CM4 */
    "pinctrl-rp1",       /* Pi 5 */
    NULL
};

#define CHIP_SCAN_MAX 16

static struct {
    int fd;              /* line request fd, or -1 */
    int active;          /* current logical state */
    char desc[PATH_MAX + 16];
    char err[PATH_MAX + 128];
} te = {
    .fd = -1,
    .active = 0,
    .desc = "",
    .err = ""
};

/**********************************************************************/

static int
chip_label_is_rpi (const char *path)
{
    struct gpiochip_info info;
    int fd, r, i;

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if ( fd < 0 )
        return 0;
    memset(&info, 0, sizeof(info));
    r = ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info);
    close(fd);
    if ( r < 0 )
        return 0;
    for (i = 0; rpi_chip_labels[i]; i++)
        if ( strcmp(info.label, rpi_chip_labels[i]) == 0 )
            return 1;
    return 0;
}

static void
find_rpi_chip (char *path, size_t sz)
{
    int i;

    for (i = 0; i < CHIP_SCAN_MAX; i++) {
        snprintf(path, sz, "/dev/gpiochip%d", i);
        if ( chip_label_is_rpi(path) )
            return;
    }
    snprintf(path, sz, "/dev/gpiochip0");
}

/* Split "[<chip>:]<line>" into a chip device path and a line
   offset. Returns 0 on success, -1 on parse error. */
static int
parse_spec (const char *spec, char *path, size_t sz, unsigned *line)
{
    const char *colon, *lstr;
    char *ep;
    unsigned long l;

    colon = strrchr(spec, ':');
    if ( colon ) {
        size_t n = colon - spec;
        if ( n == 0 )
            return -1;
        if ( spec[0] == '/' ) {
            if ( n >= sz ) return -1;
            memcpy(path, spec, n);
            path[n] = '\0';
        } else {
            if ( n + sizeof("/dev/") > sz ) return -1;
            memcpy(path, "/dev/", 5);
            memcpy(path + 5, spec, n);
            path[5 + n] = '\0';
        }
        lstr = colon + 1;
    } else {
        find_rpi_chip(path, sz);
        lstr = spec;
    }

    if ( ! *lstr )
        return -1;
    errno = 0;
    l = strtoul(lstr, &ep, 10);
    if ( errno || *ep != '\0' || l > UINT_MAX )
        return -1;
    *line = (unsigned)l;
    return 0;
}

#ifdef GPIO_V2_GET_LINE_IOCTL

static int
request_line (int chip_fd, unsigned line, int active_low)
{
    struct gpio_v2_line_request req;
    int r;

    memset(&req, 0, sizeof(req));
    req.offsets[0] = line;
    req.num_lines = 1;
    strncpy(req.consumer, TE_CONSUMER, sizeof(req.consumer) - 1);
    req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    if ( active_low )
        req.config.flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
    /* initial value: inactive (receive) */
    req.config.num_attrs = 1;
    req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    req.config.attrs[0].attr.values = 0;
    req.config.attrs[0].mask = 1;

    r = ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req);
    if ( r < 0 )
        return -1;
    return req.fd;
}

static int
set_line (int fd, int on)
{
    struct gpio_v2_line_values v;

    v.bits = on ? 1 : 0;
    v.mask = 1;
    return ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &v);
}

#else /* of GPIO_V2_GET_LINE_IOCTL */

/* Older kernel headers: use the v1 line-handle ABI */

static int
request_line (int chip_fd, unsigned line, int active_low)
{
    struct gpiohandle_request req;
    int r;

    memset(&req, 0, sizeof(req));
    req.lineoffsets[0] = line;
    req.lines = 1;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    if ( active_low )
        req.flags |= GPIOHANDLE_REQUEST_ACTIVE_LOW;
    req.default_values[0] = 0; /* inactive (receive) */
    strncpy(req.consumer_label, TE_CONSUMER,
            sizeof(req.consumer_label) - 1);

    r = ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if ( r < 0 )
        return -1;
    return req.fd;
}

static int
set_line (int fd, int on)
{
    struct gpiohandle_data d;

    memset(&d, 0, sizeof(d));
    d.values[0] = on ? 1 : 0;
    return ioctl(fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &d);
}

#endif /* of GPIO_V2_GET_LINE_IOCTL */

/**********************************************************************/

int
te_gpio_open (const char *spec, int active_low)
{
    char path[PATH_MAX];
    struct gpiochip_info info;
    unsigned line;
    int chip_fd, fd, r, e;

    if ( te.fd >= 0 )
        te_gpio_close();

    if ( parse_spec(spec, path, sizeof(path), &line) < 0 ) {
        snprintf(te.err, sizeof(te.err),
                 "invalid gpio spec \"%s\" (expected [<chip>:]<line>)", spec);
        errno = EINVAL;
        return -1;
    }

    chip_fd = open(path, O_RDONLY | O_CLOEXEC);
    if ( chip_fd < 0 ) {
        e = errno;
        snprintf(te.err, sizeof(te.err), "cannot open %s: %s",
                 path, strerror(e));
        errno = e;
        return -1;
    }

    memset(&info, 0, sizeof(info));
    r = ioctl(chip_fd, GPIO_GET_CHIPINFO_IOCTL, &info);
    if ( r == 0 && line >= info.lines ) {
        snprintf(te.err, sizeof(te.err), "%s has only %u lines (0-%u)",
                 path, info.lines, info.lines ? info.lines - 1 : 0);
        close(chip_fd);
        errno = EINVAL;
        return -1;
    }

    fd = request_line(chip_fd, line, active_low);
    e = errno;
    /* The line fd stays valid after the chip fd is closed */
    close(chip_fd);
    if ( fd < 0 ) {
        snprintf(te.err, sizeof(te.err), "cannot request %s line %u: %s",
                 path, line, strerror(e));
        errno = e;
        return -1;
    }

    te.fd = fd;
    te.active = 0;
    snprintf(te.desc, sizeof(te.desc), "%s:%u", path, line);
    return 0;
}

int
te_gpio_set (int on)
{
    on = on ? 1 : 0;
    if ( te.fd < 0 || te.active == on )
        return 0;
    if ( set_line(te.fd, on) < 0 )
        return -1;
    te.active = on;
    return 0;
}

int
te_gpio_is_active (void)
{
    return te.fd >= 0 && te.active;
}

void
te_gpio_close (void)
{
    if ( te.fd < 0 )
        return;
    set_line(te.fd, 0);
    close(te.fd);
    te.fd = -1;
    te.active = 0;
    te.desc[0] = '\0';
}

const char *
te_gpio_desc (void)
{
    return (te.fd >= 0) ? te.desc : NULL;
}

const char *
te_gpio_strerror (void)
{
    return te.err;
}

/**********************************************************************/

/*
 * Local Variables:
 * mode:c
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
