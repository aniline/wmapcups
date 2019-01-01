/* -*- Mode: C; fill-column: 79 -*-
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <stdint.h>
#include <math.h>

#include <libdockapp/dockapp.h>
#include <libdockapp/misc.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#define VERSION "0.1"
#define NAME    "wmapcups"

#include "base.xpm"

typedef struct Sprite {
    int x, y;
    int rx, ry;
    int w, h;
    int stride;
} Sprite;

/* % Charge, Big 3 digit */
Sprite sprite_bcharge  = { 22,  8,  0,  64, 10, 18, 10 };
/* Line Voltage, Small 3 digit */
Sprite sprite_linev    = { 22, 31,  0,  83,  6,  9,  7 };
/* Time left, Small 3 digit */
Sprite sprite_timeleft = { 22, 45,  0,  83,  6,  9,  7 };
/* Online/Offline indicator - 2 state */
Sprite sprite_online   = {  7, 46,  0, 108, 13,  9, 13 };
/* Charging/Not-charging indicator - 2 state, Overlaid */
Sprite sprite_charging = { 10, 21,  0,  93,  7, 14,  7 };

Sprite sprite_error    = {  0,  0, 64, 128, 64, 64, 64 };
/* Battery bar */
Sprite sprite_bar      = { 10, 13, 64,  96,  7, 28,  7 };

static const int PERIOD = 200;
static const int SIZE = 64;
static const int GLYPH_SPACE = 11;
static const int GLYPH_HYPHEN = 10;

static DAShapedPixmap *back_pm = NULL, *all_pm = NULL;
static Pixmap charge_mask = 0;
static DAProgramOption DAPoptions[] = { };

void setup(int ac, char *av[])
{
    XGCValues gcv;
    unsigned long gcm;
    back_pm = DAMakeShapedPixmapFromData(base_xpm);
    if (!back_pm)
    {
        fprintf(stderr, "ERR: Pixmap could not be loaded. Exiting\n");
        exit(EXIT_FAILURE);
    }

    /* Mask for Charge indicator. */
    charge_mask = XCreatePixmap(DADisplay, DAWindow, SIZE, SIZE, 1);
    XCopyPlane(DADisplay, back_pm->shape, charge_mask, back_pm->shapeGC, 64, 0, 64, 64, 0, 0, 1);

    /* Target drawable */
    all_pm = DAMakeShapedPixmap();
    XShapeCombineMask(DADisplay, DAWindow, ShapeBounding, 0, 0, back_pm->shape, ShapeSet);
}

void shutdown(void)
{
    DAFreeShapedPixmap(all_pm);
    DAFreeShapedPixmap(back_pm);
    XFreePixmap(DADisplay, charge_mask);
}

void clear_digits(int *digits, int num_digits)
{
    int i;
    if (digits != NULL) {
        for (i=0; i<num_digits; i++) digits[i] = 11;
    }
}

/* Returns 1 if nothing usable was placed in digits buffer, 0 otherwise */
int to_digits(int val, int *sign, int *digits, int num_digits)
{
    int i;
    unsigned int abs_val = abs(val);

    if (sign != NULL)
    {
        *sign = val < 0 ? -1 : 1;
    }

    if (digits == NULL)
    {
        return 1;
    }
    else
    {
        char s_num[32] = "";
        int s_num_n = snprintf(s_num, 32, "%d", abs_val);

        clear_digits(digits, num_digits);

        if (s_num_n > num_digits)
        {
            return 0;
        }

        for (i=0; i<s_num_n; i++) {
            char v = s_num[i];

            if ((v >= '0') && (v <= '9'))
            {
                digits [num_digits - s_num_n + i] = v-'0';
            } else if (v == '-')
            {
                digits [num_digits - s_num_n + i] = 10;
            }
        }
    }
    return 0;
}

/* Blits 3digit number (v) based on source (font) and destination indicated by
 * Sprite 'sp' */
void show_num(int v, Sprite *sp)
{
    int digits_size = 3;
    int digits[digits_size];
    int sign = 1;

    clear_digits(digits, digits_size);
    if (0 == to_digits(v, &sign, digits, digits_size)) {
        int i;
        for (i=0; i<digits_size; i++) {
            DASPCopyArea(back_pm, all_pm,
                         (digits[i] * (sp->stride)) + sp->rx, sp->ry,
                         sp->w, sp->h,
                         sp->x + (i* (sp->stride)), sp->y);
        }
    }
}

/* Toggles the online/offline sprite based on 'online' parameter */
void show_linestatus(int online)
{
    DASPCopyArea(back_pm, all_pm,
                 sprite_online.rx + ((online ? 1: 0) * sprite_online.w), sprite_online.ry,
                 sprite_online.w, sprite_online.h,
                 sprite_online.x, sprite_online.y);
}

/* Overlays the battery motif with a lighting sprite if the UPS battery
 * is charging */
int show_charging(int charging)
{
    if (charging) {
        XSetClipMask(DADisplay, DAGC, charge_mask);
        DASPCopyArea(back_pm, all_pm,
                     sprite_charging.rx, sprite_charging.ry,
                     sprite_charging.w, sprite_charging.h,
                     sprite_charging.x, sprite_charging.y);
        XSetClipMask(DADisplay, DAGC, all_pm->shape);
    }
}

int show_charge_bar(int v)
{
    Sprite sprite = sprite_bar;
    int offs = 4, height = sprite.h;

    if ((v >= 0) && (v <= 100)) {
        height = floor((sprite.h * v) / 100);
        if (v > 60) offs = 0;
        else if (v > 40) offs = 1;
        else if (v > 20) offs = 2;
        else offs = 3;
    }

    DASPCopyArea(back_pm, all_pm,
                 sprite.rx + (offs * sprite.stride), sprite.ry,
                 sprite.w, height,
                 sprite.x, sprite.y + (sprite.h - height));
}

/* Shows an error banner */
void show_error()
{
    DASPCopyArea(back_pm, all_pm,
                 sprite_error.rx, sprite_error.ry,
                 sprite_error.w, sprite_error.h,
                 sprite_error.x, sprite_error.y);
}

void test()
{
static int test_val = 0;
static int test_blink = 1;
static int test_blink_counter = 0;
static int test_error = 0;

    test_val += 3;
    test_val = test_val % 101;

    show_num(test_val * 20, &sprite_linev);
    show_num(test_val * 20, &sprite_timeleft);
    show_num(test_val, &sprite_bcharge);

    test_blink_counter = test_blink_counter + 1;
    if (test_blink_counter > 10) {
        test_blink_counter = 0;
        test_blink ^= 1;
        test_error = test_error + 1;
    }
    show_linestatus(test_blink);
    show_charging(test_blink);
    show_charge_bar(test_val);

    /* if ((test_error % 4) == 0) { */
    /*     show_error(); */
    /* } */
}

void update()
{
    DASPCopyArea(back_pm, all_pm, 0, 0, SIZE, SIZE, 0, 0);

    test();

    DASPSetPixmap(all_pm);
}

int main(int argc, char *argv[])
{
    DACallbacks eventCallbacks = {
        shutdown,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        update,
    };

    DAParseArguments(argc, argv, DAPoptions,
                     sizeof(DAPoptions)/sizeof(DAProgramOption),
                     "APC UPS status, Anil N <anilknyn@yahoo.com>\n",
                     "This is " NAME " " VERSION "\n");

    DASetExpectedVersion(20050716);
    DAInitialize("", NAME, 64, 64, argc, argv);
    setup(argc, argv);

    DASetCallbacks(&eventCallbacks);
    DASetTimeout(PERIOD);
    DAShow();
    DAEventLoop();

    return EXIT_SUCCESS;
}