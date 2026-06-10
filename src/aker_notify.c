/**
 * Copyright 2017 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include <msgpack.h>
#include <wrp-c/wrp-c.h>
#include <libparodus.h>

#include "aker_notify.h"
#include "aker_log.h"
#include "aker_mem.h"

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
static libpd_instance_t s_pd_instance = NULL;
static const char      *s_device_id   = NULL;

/*----------------------------------------------------------------------------*/
/*                             Internal Helpers                               */
/*----------------------------------------------------------------------------*/

static const char *event_type_str( aker_event_type_t t )
{
    switch( t ) {
        case AKER_EVENT_DOWNTIME_STARTING_SOON: return "DOWNTIME_STARTING_SOON";
        case AKER_EVENT_DOWNTIME_STARTED:       return "DOWNTIME_STARTED";
        case AKER_EVENT_DOWNTIME_ENDING_SOON:   return "DOWNTIME_ENDING_SOON";
        case AKER_EVENT_DOWNTIME_ENDED:         return "DOWNTIME_ENDED";
        case AKER_EVENT_PROFILE_UNPAUSED:       return "PROFILE_UNPAUSED";
        default:                                return "UNKNOWN";
    }
}

static void pack_str( msgpack_packer *pk, const char *s )
{
    size_t len = s ? strlen(s) : 0;
    msgpack_pack_str(pk, len);
    if( len > 0 ) {
        msgpack_pack_str_body(pk, s, len);
    }
}

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

void aker_notify_init( libpd_instance_t instance, const char *device_id )
{
    s_pd_instance = instance;
    s_device_id   = device_id;
}

int aker_generate_uuid( char *buf )
{
    uint8_t bytes[16];
    FILE   *f;
    size_t  n;

    f = fopen("/dev/urandom", "rb");
    if( NULL == f ) {
        debug_error("aker_generate_uuid: failed to open /dev/urandom\n");
        return -1;
    }

    n = fread(bytes, 1, sizeof(bytes), f);
    fclose(f);

    if( n != sizeof(bytes) ) {
        debug_error("aker_generate_uuid: short read from /dev/urandom\n");
        return -1;
    }

    /* Set version 4 and variant bits per RFC 4122 */
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;

    snprintf(buf, 37,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x"
             "-%02x%02x%02x%02x%02x%02x",
             bytes[0],  bytes[1],  bytes[2],  bytes[3],
             bytes[4],  bytes[5],  bytes[6],  bytes[7],
             bytes[8],  bytes[9],  bytes[10], bytes[11],
             bytes[12], bytes[13], bytes[14], bytes[15]);
    return 0;
}

void aker_format_iso8601( time_t t, char *buf )
{
    struct tm tm_val;
    gmtime_r(&t, &tm_val);
    strftime(buf, 21, "%Y-%m-%dT%H:%M:%SZ", &tm_val);
}

/* Count how many MACs appear in a whitespace/comma-separated string. */
static int count_macs( const char *macs )
{
    int count = 0;
    const char *p = macs;
    int in_token = 0;

    if( !macs || !*macs ) return 0;

    while( *p ) {
        if( *p == ' ' || *p == ',' ) {
            in_token = 0;
        } else {
            if( !in_token ) {
                count++;
                in_token = 1;
            }
        }
        p++;
    }
    return count;
}

size_t aker_notify_pack( const aker_notify_ctx_t *ctx, void **out_buf )
{
    msgpack_sbuffer sbuf;
    msgpack_packer  pk;
    char            event_id[37];
    char            notify_time[21];
    char            sched_time[21];
    char            pause_time[21];
    const char     *type_str;
    int             is_unpaused;
    int             num_fields;
    int             mac_count;
    size_t          result = 0;
    time_t          now;

    if( 0 != aker_generate_uuid(event_id) ) {
        return 0;
    }

    now = time(NULL);
    aker_format_iso8601(now, notify_time);
    type_str   = event_type_str(ctx->event_type);
    is_unpaused = (ctx->event_type == AKER_EVENT_PROFILE_UNPAUSED);

    /* Base fields: eventType, transactionId, eventId, notificationTime,
     *              timeZone, affectedMacs = 6 map entries.
     * Plus either scheduledTime (downtime events) or pauseUntilTime (unpause). */
    num_fields = 7; /* 6 base + 1 conditional */

    mac_count = count_macs(ctx->affected_macs);

    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_map(&pk, (uint32_t)num_fields);

    /* eventType */
    pack_str(&pk, "eventType");
    pack_str(&pk, type_str);

    /* transactionId */
    pack_str(&pk, "transactionId");
    pack_str(&pk, ctx->transaction_id ? ctx->transaction_id : "");

    /* eventId */
    pack_str(&pk, "eventId");
    pack_str(&pk, event_id);

    /* notificationTime */
    pack_str(&pk, "notificationTime");
    pack_str(&pk, notify_time);

    /* timeZone */
    pack_str(&pk, "timeZone");
    pack_str(&pk, ctx->time_zone ? ctx->time_zone : "");

    /* affectedMacs — encode as a msgpack array of strings */
    pack_str(&pk, "affectedMacs");
    msgpack_pack_array(&pk, (uint32_t)mac_count);
    if( mac_count > 0 && ctx->affected_macs ) {
        /* Walk tokens and pack each mac individually */
        char *copy = strdup(ctx->affected_macs);
        if( copy ) {
            char *tok;
            char *rest = copy;
            while( (tok = strtok_r(rest, " ,", &rest)) != NULL ) {
                pack_str(&pk, tok);
            }
            free(copy);
        }
    }

    /* Conditional time field */
    if( is_unpaused ) {
        aker_format_iso8601(ctx->pause_until_time, pause_time);
        pack_str(&pk, "pauseUntilTime");
        pack_str(&pk, pause_time);
    } else {
        aker_format_iso8601(ctx->scheduled_time, sched_time);
        pack_str(&pk, "scheduledTime");
        pack_str(&pk, sched_time);
    }

    if( sbuf.data ) {
        *out_buf = aker_malloc(sbuf.size);
        if( *out_buf ) {
            memcpy(*out_buf, sbuf.data, sbuf.size);
            result = sbuf.size;
        }
    }
    msgpack_sbuffer_destroy(&sbuf);
    return result;
}

void aker_notify_send( const aker_notify_ctx_t *ctx )
{
    char        dest[64];
    void       *payload    = NULL;
    size_t      payload_sz;
    wrp_msg_t   msg;
    int         rv;

    if( !s_pd_instance || !s_device_id ) {
        debug_error("aker_notify_send: not initialized\n");
        return;
    }

    payload_sz = aker_notify_pack(ctx, &payload);
    if( 0 == payload_sz ) {
        debug_error("aker_notify_send: failed to pack payload\n");
        return;
    }

    snprintf(dest, sizeof(dest), "mac:%s/aker/schedule/event", s_device_id);

    memset(&msg, 0, sizeof(msg));
    msg.msg_type             = WRP_MSG_TYPE__EVENT;
    msg.u.event.source       = "aker";
    msg.u.event.dest         = dest;
    msg.u.event.content_type = "application/msgpack";
    msg.u.event.payload      = payload;
    msg.u.event.payload_size = payload_sz;

    rv = libparodus_send(s_pd_instance, &msg);
    if( 0 != rv ) {
        debug_error("aker_notify_send: libparodus_send failed (%d) for event %s\n",
                    rv, event_type_str(ctx->event_type));
    } else {
        debug_info("aker_notify_send: sent %s notification\n",
                   event_type_str(ctx->event_type));
    }

    aker_free(payload);
}
