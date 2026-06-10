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
#ifndef __AKER_NOTIFY_H__
#define __AKER_NOTIFY_H__

#include <time.h>
#include <libparodus.h>

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define NOTIFY_PRE_WINDOW_SECS  900   /* 15 minutes */
#define NOTIFY_TIMING_TOLERANCE  60   /* ±60 seconds */

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/

typedef enum {
    AKER_EVENT_DOWNTIME_STARTING_SOON = 0,
    AKER_EVENT_DOWNTIME_STARTED,
    AKER_EVENT_DOWNTIME_ENDING_SOON,
    AKER_EVENT_DOWNTIME_ENDED,
    AKER_EVENT_PROFILE_UNPAUSED
} aker_event_type_t;

typedef struct {
    aker_event_type_t event_type;
    const char       *transaction_id;   /* borrowed — do not free */
    time_t            scheduled_time;   /* 0 for PROFILE_UNPAUSED */
    time_t            pause_until_time; /* only for PROFILE_UNPAUSED */
    const char       *time_zone;        /* borrowed — do not free */
    const char       *affected_macs;    /* comma-separated or space-sep string */
    const char       *device_id;        /* gateway mac — borrowed */
} aker_notify_ctx_t;

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/

/**
 *  Initialise the notification subsystem with the libparodus instance and
 *  device identifier.  Must be called before aker_notify_send().
 *
 *  @param instance   the active libparodus handle
 *  @param device_id  the gateway MAC string (e.g. "112233445566")
 */
void aker_notify_init( libpd_instance_t instance, const char *device_id );

/**
 *  Build a msgpack-encoded WRP event message and dispatch it via libparodus.
 *  Fire-and-forget: on send failure the error is logged and execution continues.
 *
 *  @param ctx  pointer to a filled aker_notify_ctx_t describing the event
 */
void aker_notify_send( const aker_notify_ctx_t *ctx );

/**
 *  Generate a UUID v4 string from /dev/urandom.
 *
 *  @param buf  output buffer, must be at least 37 bytes
 *  @return 0 on success, -1 on failure
 */
int aker_generate_uuid( char *buf );

/**
 *  Format a time_t as an ISO 8601 UTC string ("YYYY-MM-DDTHH:MM:SSZ").
 *
 *  @param t    the unix timestamp to format
 *  @param buf  output buffer, must be at least 21 bytes
 */
void aker_format_iso8601( time_t t, char *buf );

/**
 *  Encode a notification payload into msgpack.
 *
 *  @param ctx       the notification context
 *  @param out_buf   set to the allocated msgpack buffer on success (caller frees)
 *  @return          size of the encoded buffer, 0 on failure
 */
size_t aker_notify_pack( const aker_notify_ctx_t *ctx, void **out_buf );

#endif /* __AKER_NOTIFY_H__ */
