/* -*- Mode: C; fill-column: 79 -*-
 * Interface to upsfetch.c used by the dockapp.
 * Copyright (C) 2019 Anil N <anilknyn@yahoo.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef UPS_FETCH_H
#define UPS_FETCH_H

#define BOOL_STR(v) ((v) ? "True" : "False")

typedef enum {
    FE_OK = 0,
    FE_TIMEOUT = 1,
    FE_SENDERROR = 2,
    FE_ERROR = 3,
    FE_TOOBIG = 4,
} FETCH_ERROR;

typedef enum {
    STAT_LINEV = 0,
    STAT_CHARGE,
    STAT_CHARGING,
    STAT_ONLINE,
    STAT_LOADPCT,
    STAT_TIMELEFT,
    STAT_MAX,
} UPSStatusFields;

#define FIELD_BIT(x) (1 << (x))
#define EXPECTED_FIELDS (\
    FIELD_BIT(STAT_LINEV)   |\
    FIELD_BIT(STAT_CHARGE)  |\
    FIELD_BIT(STAT_LOADPCT) |\
    FIELD_BIT(STAT_ONLINE) |\
    FIELD_BIT(STAT_TIMELEFT) )

typedef union {
    int   i;
    float f; /* Not used, currently */
} UPSStatusField;

typedef struct {
    UPSStatusField fields[STAT_MAX];
    time_t         upd_time;
    unsigned int   field_bitmap;
} UPSStatus;

extern UPSStatus ups;

void  dumpStat(UPSStatus *u);
int   get_status_from_apc_nis_server(const char *host, unsigned short port);

#endif
