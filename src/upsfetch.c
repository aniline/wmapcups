/* -*- Mode: C; fill-column: 79 -*-
 * Grab and parse 'status' response from apcupsd.
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

#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "upsfetch.h"

/* Global stat */
UPSStatus ups = {0};
/* 'Back-buffer' for updating */
static UPSStatus ups_updating = {0};

#define LABEL_LEN_MAX  20
#define SEARCH_TOKEN_LEN_MAX  20
#define FIELD_LABEL_MAX 5

typedef struct
{
    char            label[LABEL_LEN_MAX];
    UPSStatusFields index;
    int             parse_num;
    int             is_float; /* Not used */
    char            search_token[SEARCH_TOKEN_LEN_MAX];
} FieldLabelMap;

FieldLabelMap field_labels[5] =
{
    { "LINEV",    STAT_LINEV   , 1, 0, "" },
    { "BCHARGE",  STAT_CHARGE  , 1, 0, "" },
    { "STATUS",   STAT_ONLINE  , 0, 0, "ONLINE" },
    { "LOADPCT",  STAT_LOADPCT , 1, 0, "" },
    { "TIMELEFT", STAT_TIMELEFT, 1, 0, "" },
};

/* Used for debugging connection, lanuch dockapp with -t option */
void dumpStat(UPSStatus *u)
{
    printf("\nGrabbed stats:\n");
    printf("------------------------\n");
    printf("Line Voltage [%d]: %d\n", STAT_LINEV, u->fields[STAT_LINEV].i);
    printf("Charge       [%d]: %d\n", STAT_CHARGE, u->fields[STAT_CHARGE].i);
    printf("Load Percent [%d]: %d\n", STAT_LOADPCT, u->fields[STAT_LOADPCT].i);
    printf("Time left(m) [%d]: %d\n", STAT_TIMELEFT, u->fields[STAT_TIMELEFT].i);
    printf("Online       [%d]: %s\n", STAT_ONLINE, BOOL_STR(u->fields[STAT_ONLINE].i));
    printf("Charging     [%d]: %s\n", STAT_CHARGING, BOOL_STR(u->fields[STAT_CHARGING].i));
    printf("------------------------\n\n");
}

void deriveDerivedFields(UPSStatus *u)
{
    u->fields[STAT_CHARGING].i =
        ((u->fields[STAT_CHARGE].i < 100) && (u->fields[STAT_ONLINE].i != 0));
    time(&u->upd_time);
}

void clearStat(UPSStatus *u)
{
    memset(&u, 0, sizeof(UPSStatus));
}

/* Updates ups_updating.fields when called with interesting status
 * lines */
int grok_line(char *p_line)
{
    char *token = NULL, *save = NULL, *e = NULL;
    char *line = p_line;
    char *delim = ": ";
    int  t_count = 0, i = 0, v = 0, j = 0;

#define STRING_MAX     100
#define NUM_TOKEN_MAX  10

    char tokens[NUM_TOKEN_MAX][STRING_MAX];

    token = strtok_r(line, delim, &save);
    while ((token != NULL) && t_count < NUM_TOKEN_MAX)
    {
        strncpy(tokens[t_count], token, STRING_MAX);
        token = strtok_r(NULL, delim, &save);
        t_count ++;
    }

    /* Atleast 2 tokens */
    if (t_count > 1)
    {
        for (i=0; i<5; i++)
        {
            if (0 == strncmp(field_labels[i].label, tokens[0], LABEL_LEN_MAX))
            {
                if (field_labels[i].parse_num) /* Number to be parsed */
                {
                    v = strtol(tokens[1], &e, 10);
                    if (e == tokens[1]) {
                        fprintf(stderr, "Parse error on "
                                "field '%s' => '%s' as integer",
                                tokens[0], tokens[1]);
                    }
                    else
                    {
                        ups_updating.fields[field_labels[i].index].i = v;
                        ups_updating.field_bitmap |= (1 << field_labels[i].index);
                    }
                }
                else /* Presense of a string in value */
                {
                    ups_updating.fields[field_labels[i].index].i = 0;
                    for (j=1; j<t_count; j++)
                    {
                        ups_updating.fields[field_labels[i].index].i =
                            ups_updating.fields[field_labels[i].index].i
                            || (NULL != strstr(tokens[j],field_labels[i].search_token));
                    }
                    ups_updating.field_bitmap |= (1 << field_labels[i].index);
                }
            }
        }
    }
}

/* Asks for 'status' and processes the lines that come back, with grok_line */
FETCH_ERROR get_status(int sfd)
{
#define LINE_BUF_SIZE 2048
    FETCH_ERROR rc = FE_ERROR;
    int  msgsize = 0, done = 0;;
    char error_msg[512];
    char msgbuf[LINE_BUF_SIZE] = "\x00\x06status";

    if (send(sfd, msgbuf, 8, 0) == -1)
    {
        fprintf(stderr, "send: %s\n", strerror_r(errno, error_msg, 512));
        rc = FE_SENDERROR;
    }
    clearStat(&ups_updating);

    while (!done)
    {
        char sizebuf[2];
        int  size = 0;

        if (recv(sfd, sizebuf, 2, 0) == -1)
        {
            strerror_r(errno, error_msg, 512);
            fprintf(stderr, "recv: %s\n", error_msg);
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                rc = FE_TIMEOUT;
            goto Error;
        }

        size = (sizebuf[0] << 1) | (sizebuf[1]);
        if (size < LINE_BUF_SIZE) {
            if (size == 0) /* End message */
            {
                done = 1;
                rc = FE_OK;
            }
            else
            {
                if (recv(sfd, msgbuf, size, 0) == -1) {
                    fprintf(stderr, "recv: %s\n", strerror_r(errno, error_msg, 512));
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                        rc = FE_TIMEOUT;
                    goto Error;
                }
                msgbuf[size] = 0x00;
                grok_line(msgbuf);
            }
        }
        else /* NIS says message Too big  */
        {
            rc = FE_TOOBIG;
        }
    }

    if (rc == FE_OK)
    {
        if ((ups_updating.field_bitmap & EXPECTED_FIELDS)
            == EXPECTED_FIELDS)
        {
            deriveDerivedFields(&ups_updating);
            memcpy(&ups, &ups_updating, sizeof(ups));
        }
        else
        {
            fprintf(stderr, "Could not grab all the expected fields, not updating");
        }
    }

Error:
    return rc;
}

/** Entry point to fetching stats. Tries to connect to apcupsd running
 *  on host `hostname` set to listen on port `port` in the 'NIS' mode.
 */
int get_status_from_apc_nis_server(const char *hostname, unsigned short port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd = -1, s;
    FETCH_ERROR fe = FE_ERROR;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    s = getaddrinfo(hostname, NULL, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        struct sockaddr addr;
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        memcpy(&addr, rp->ai_addr, rp->ai_addrlen);

        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO,
                       (const char*)&tv, sizeof tv) == -1) {
            perror("setsockopt");
            goto Error;
        }

        (*(struct sockaddr_in*)(&addr)).sin_port = htons(port);
        if (connect(sfd, &addr, rp->ai_addrlen) != -1)
            break;

        perror("connect");

        close(sfd);
        sfd = -1;
    }

    if (sfd != -1) {
        fe = get_status(sfd);
    }

Error:
    if (sfd != -1) {
        close(sfd);
    }
    return (fe == FE_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
