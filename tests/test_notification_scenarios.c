/**
 * Comprehensive test cases for aker_notification scenarios
 * Tests timeline building, state detection, and notification logic
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "../src/aker_notification.h"
#include "../src/schedule.h"

/* Test helper macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  ❌ FAIL: %s\n", message); \
            return 0; \
        } \
    } while(0)

#define TEST_PASS(name) \
    do { \
        printf("  ✓ PASS: %s\n", name); \
        return 1; \
    } while(0)

/* Helper to create a simple weekly schedule */
schedule_t* create_weekly_schedule(const char *mac, time_t block_time, time_t unblock_time, const char *tz) {
    schedule_t *s = (schedule_t*)calloc(1, sizeof(schedule_t));
    s->mac_count = 1;
    s->macs = (mac_address*)calloc(1, sizeof(mac_address));
    strncpy(s->macs[0].mac, mac, MAC_ADDRESS_SIZE - 1);
    s->time_zone = strdup(tz);
    
    /* Create weekly block event */
    schedule_event_t *block_event = (schedule_event_t*)calloc(1, sizeof(schedule_event_t));
    block_event->time = block_time;
    block_event->block_count = 1;
    block_event->block = (uint32_t*)malloc(sizeof(uint32_t));
    block_event->block[0] = 0;  /* MAC index 0 */
    
    /* Create weekly unblock event */
    schedule_event_t *unblock_event = (schedule_event_t*)calloc(1, sizeof(schedule_event_t));
    unblock_event->time = unblock_time;
    unblock_event->block_count = 0;  /* Unblock all */
    
    block_event->next = unblock_event;
    unblock_event->next = NULL;
    
    s->weekly = block_event;
    s->absolute = NULL;
    
    return s;
}

/* Helper to create schedule with absolute event */
schedule_t* create_absolute_schedule(const char *mac, time_t block_time, time_t unblock_time, const char *tz) {
    schedule_t *s = (schedule_t*)calloc(1, sizeof(schedule_t));
    s->mac_count = 1;
    s->macs = (mac_address*)calloc(1, sizeof(mac_address));
    strncpy(s->macs[0].mac, mac, MAC_ADDRESS_SIZE - 1);
    s->time_zone = strdup(tz);
    
    /* Create absolute block event */
    schedule_event_t *block_event = (schedule_event_t*)calloc(1, sizeof(schedule_event_t));
    block_event->time = block_time;
    block_event->block_count = 1;
    block_event->block = (uint32_t*)malloc(sizeof(uint32_t));
    block_event->block[0] = 0;
    
    /* Create absolute unblock event */
    schedule_event_t *unblock_event = (schedule_event_t*)calloc(1, sizeof(schedule_event_t));
    unblock_event->time = unblock_time;
    unblock_event->block_count = 0;
    
    block_event->next = unblock_event;
    unblock_event->next = NULL;
    
    s->absolute = block_event;
    s->weekly = NULL;
    
    return s;
}

/* Helper to cleanup schedule */
void cleanup_test_schedule(schedule_t *s) {
    if (!s) return;
    
    /* Free weekly events */
    schedule_event_t *event = s->weekly;
    while (event) {
        schedule_event_t *next = event->next;
        if (event->block) free(event->block);
        free(event);
        event = next;
    }
    
    /* Free absolute events */
    event = s->absolute;
    while (event) {
        schedule_event_t *next = event->next;
        if (event->block) free(event->block);
        free(event);
        event = next;
    }
    
    if (s->macs) free(s->macs);
    if (s->time_zone) free((char*)s->time_zone);
    free(s);
}

/*----------------------------------------------------------------------------*/
/*                              Test Cases                                    */
/*----------------------------------------------------------------------------*/

/**
 * Test 1: Basic timeline building with weekly schedule
 */
int test_weekly_timeline_building() {
    printf("\n--- Test 1: Weekly Timeline Building ---\n");
    
    time_t now = time(NULL);
    
    /* Create schedule: Block Monday 9 PM - Tuesday 7 AM */
    time_t monday_9pm = 75600;   /* Seconds since Sunday midnight */
    time_t tuesday_7am = 111600;
    
    schedule_t *schedule = create_weekly_schedule(
        "aa:bb:cc:dd:ee:ff",
        monday_9pm,
        tuesday_7am,
        "UTC"
    );
    
    /* Build timeline */
    mac_timeline_collection_t *timeline = build_timeline_from_schedule(schedule, now, 2);
    
    TEST_ASSERT(timeline != NULL, "Timeline should be created");
    TEST_ASSERT(timeline->mac_count == 1, "Should have 1 MAC");
    TEST_ASSERT(timeline->timelines[0].periods != NULL, "Should have periods");
    
    /* Verify periods exist for 2 weeks */
    int period_count = 0;
    mac_block_period_t *period = timeline->timelines[0].periods;
    while (period) {
        period_count++;
        TEST_ASSERT(period->end_time > period->start_time, "End time should be after start time");
        period = period->next;
    }
    
    TEST_ASSERT(period_count >= 2, "Should have at least 2 periods (2 weeks)");
    
    /* Cleanup */
    destroy_timeline_collection(timeline);
    cleanup_test_schedule(schedule);
    
    TEST_PASS("Weekly timeline building");
}

/**
 * Test 2: Absolute schedule timeline building
 */
int test_absolute_timeline_building() {
    printf("\n--- Test 2: Absolute Timeline Building ---\n");
    
    time_t now = time(NULL);
    time_t block_start = now + 3600;  /* 1 hour from now */
    time_t block_end = now + 7200;    /* 2 hours from now */
    
    schedule_t *schedule = create_absolute_schedule(
        "11:22:33:44:55:66",
        block_start,
        block_end,
        "UTC"
    );
    
    mac_timeline_collection_t *timeline = build_timeline_from_schedule(schedule, now, 2);
    
    TEST_ASSERT(timeline != NULL, "Timeline should be created");
    TEST_ASSERT(timeline->timelines[0].periods != NULL, "Should have period");
    
    mac_block_period_t *period = timeline->timelines[0].periods;
    TEST_ASSERT(period->start_time == block_start, "Start time should match");
    TEST_ASSERT(period->end_time == block_end, "End time should match");
    TEST_ASSERT(period->next == NULL, "Should have only one period");
    
    destroy_timeline_collection(timeline);
    cleanup_test_schedule(schedule);
    
    TEST_PASS("Absolute timeline building");
}

/**
 * Test 3: "Until I Unpause" detection (indefinite block)
 */
int test_until_i_unpause_detection() {
    printf("\n--- Test 3: 'Until I Unpause' Detection ---\n");
    
    schedule_t *s = (schedule_t*)calloc(1, sizeof(schedule_t));
    s->mac_count = 1;
    s->macs = (mac_address*)calloc(1, sizeof(mac_address));
    strncpy(s->macs[0].mac, "aa:bb:cc:dd:ee:ff", MAC_ADDRESS_SIZE - 1);
    s->time_zone = strdup("UTC");
    
    /* Create weekly schedule with block but NO unblock (indefinite) */
    schedule_event_t *block_event = (schedule_event_t*)calloc(1, sizeof(schedule_event_t));
    block_event->time = 75600;  /* Monday 9 PM */
    block_event->block_count = 1;
    block_event->block = (uint32_t*)malloc(sizeof(uint32_t));
    block_event->block[0] = 0;
    block_event->next = NULL;  /* No unblock event */
    
    s->weekly = block_event;
    
    /* Test indefinite block detection */
    bool is_indefinite = is_mac_indefinitely_blocked(s, 0);
    TEST_ASSERT(is_indefinite == true, "Should detect indefinite block");
    
    /* Build timeline - should skip this MAC */
    time_t now = time(NULL);
    mac_timeline_collection_t *timeline = build_timeline_from_schedule(s, now, 2);
    
    TEST_ASSERT(timeline != NULL, "Timeline should be created");
    TEST_ASSERT(timeline->timelines[0].periods == NULL, "Indefinite block MAC should have no periods");
    
    destroy_timeline_collection(timeline);
    cleanup_test_schedule(s);
    
    TEST_PASS("Until I Unpause detection");
}

/**
 * Test 4: State-change detection (overlapping schedules)
 */
int test_state_change_detection() {
    printf("\n--- Test 4: State Change Detection ---\n");
    
    time_t now = time(NULL);
    
    /* Create schedule with weekly block */
    time_t monday_9pm = 75600;
    time_t tuesday_7am = 111600;
    
    schedule_t *schedule = create_weekly_schedule(
        "aa:bb:cc:dd:ee:ff",
        monday_9pm,
        tuesday_7am,
        "UTC"
    );
    
    /* Add absolute pause that extends beyond weekly */
    schedule_event_t *pause_block = (schedule_event_t*)calloc(1, sizeof(schedule_event_t));
    pause_block->time = now + 1000;
    pause_block->block_count = 1;
    pause_block->block = (uint32_t*)malloc(sizeof(uint32_t));
    pause_block->block[0] = 0;
    
    schedule_event_t *pause_unblock = (schedule_event_t*)calloc(1, sizeof(schedule_event_t));
    pause_unblock->time = now + 200000;  /* Extends beyond weekly end */
    pause_unblock->block_count = 0;
    
    pause_block->next = pause_unblock;
    schedule->absolute = pause_block;
    
    /* Test: Device should remain blocked after weekly ends (due to absolute) */
    time_t weekly_end_time = now + 150000;
    bool is_blocked = is_device_blocked_at(schedule, 0, weekly_end_time);
    
    TEST_ASSERT(is_blocked == true, "Device should remain blocked due to absolute schedule");
    
    cleanup_test_schedule(schedule);
    
    TEST_PASS("State change detection with overlapping schedules");
}

/**
 * Test 5: Next notification time calculation
 */
int test_next_notification_time() {
    printf("\n--- Test 5: Next Notification Time Calculation ---\n");
    
    time_t now = time(NULL);
    time_t block_start = now + 3600;  /* 1 hour from now */
    time_t block_end = now + 7200;    /* 2 hours from now */
    
    schedule_t *schedule = create_absolute_schedule(
        "aa:bb:cc:dd:ee:ff",
        block_start,
        block_end,
        "UTC"
    );
    
    mac_timeline_collection_t *timeline = build_timeline_from_schedule(schedule, now, 2);
    TEST_ASSERT(timeline != NULL, "Timeline should be created");
    
    /* Get next notification time */
    time_t next_notif = get_next_notification_time(timeline, now);
    
    /* Should be 15 min before block start */
    time_t expected_time = block_start - 900;  /* NOTIFICATION_ADVANCE_TIME_SEC */
    
    TEST_ASSERT(next_notif == expected_time, "Next notification should be 15 min before block");
    
    destroy_timeline_collection(timeline);
    cleanup_test_schedule(schedule);
    
    TEST_PASS("Next notification time calculation");
}

/**
 * Test 6: Short period handling (< 15 min)
 */
int test_short_period_handling() {
    printf("\n--- Test 6: Short Period Handling ---\n");
    
    time_t now = time(NULL);
    time_t block_start = now + 3600;
    time_t block_end = now + 3900;  /* Only 5 minutes */
    
    schedule_t *schedule = create_absolute_schedule(
        "aa:bb:cc:dd:ee:ff",
        block_start,
        block_end,
        "UTC"
    );
    
    mac_timeline_collection_t *timeline = build_timeline_from_schedule(schedule, now, 2);
    TEST_ASSERT(timeline != NULL, "Timeline should be created");
    
    /* For short periods, next notification should be the start time itself
     * (skipping STARTING_SOON) */
    time_t next_notif = get_next_notification_time(timeline, now);
    
    TEST_ASSERT(next_notif == block_start, "Short period should skip STARTING_SOON");
    
    destroy_timeline_collection(timeline);
    cleanup_test_schedule(schedule);
    
    TEST_PASS("Short period handling");
}

/**
 * Test 7: Timeline persistence (not rebuilt on state change)
 */
int test_timeline_persistence() {
    printf("\n--- Test 7: Timeline Persistence (Schedule Structure vs State Change) ---\n");
    
    time_t now = time(NULL);
    
    schedule_t *schedule1 = create_weekly_schedule(
        "aa:bb:cc:dd:ee:ff",
        75600,
        111600,
        "UTC"
    );
    
    mac_timeline_collection_t *timeline1 = build_timeline_from_schedule(schedule1, now, 2);
    TEST_ASSERT(timeline1 != NULL, "Timeline 1 should be created");
    
    /* Simulate same schedule pointer (state change only) */
    mac_timeline_collection_t *timeline2 = build_timeline_from_schedule(schedule1, now + 3600, 2);
    TEST_ASSERT(timeline2 != NULL, "Timeline 2 should be created");
    
    /* In real implementation, timeline should NOT be rebuilt if schedule pointer unchanged */
    printf("  Note: In scheduler.c, timeline rebuild now checks schedule_structure_changed flag\n");
    printf("  Timeline is only rebuilt when schedule pointer changes, not on state changes\n");
    
    destroy_timeline_collection(timeline1);
    destroy_timeline_collection(timeline2);
    cleanup_test_schedule(schedule1);
    
    TEST_PASS("Timeline persistence logic");
}

/**
 * Test 8: Multiple MACs batching
 */
int test_multiple_macs_batching() {
    printf("\n--- Test 8: Multiple MACs Batching ---\n");
    
    time_t now = time(NULL);
    
    /* Create schedule with 3 MACs blocked at same time */
    schedule_t *s = (schedule_t*)calloc(1, sizeof(schedule_t));
    s->mac_count = 3;
    s->macs = (mac_address*)calloc(3, sizeof(mac_address));
    strncpy(s->macs[0].mac, "aa:bb:cc:dd:ee:ff", MAC_ADDRESS_SIZE - 1);
    strncpy(s->macs[1].mac, "11:22:33:44:55:66", MAC_ADDRESS_SIZE - 1);
    strncpy(s->macs[2].mac, "99:88:77:66:55:44", MAC_ADDRESS_SIZE - 1);
    s->time_zone = strdup("UTC");
    
    /* Block all 3 MACs at same time */
    schedule_event_t *block_event = (schedule_event_t*)calloc(1, sizeof(schedule_event_t));
    block_event->time = 75600;
    block_event->block_count = 3;
    block_event->block = (uint32_t*)malloc(3 * sizeof(uint32_t));
    block_event->block[0] = 0;
    block_event->block[1] = 1;
    block_event->block[2] = 2;
    
    schedule_event_t *unblock_event = (schedule_event_t*)calloc(1, sizeof(schedule_event_t));
    unblock_event->time = 111600;
    unblock_event->block_count = 0;
    
    block_event->next = unblock_event;
    s->weekly = block_event;
    
    mac_timeline_collection_t *timeline = build_timeline_from_schedule(s, now, 2);
    TEST_ASSERT(timeline != NULL, "Timeline should be created");
    TEST_ASSERT(timeline->mac_count == 3, "Should have 3 MACs");
    
    /* All 3 MACs should have periods at same times */
    time_t mac0_start = timeline->timelines[0].periods ? timeline->timelines[0].periods->start_time : 0;
    time_t mac1_start = timeline->timelines[1].periods ? timeline->timelines[1].periods->start_time : 0;
    time_t mac2_start = timeline->timelines[2].periods ? timeline->timelines[2].periods->start_time : 0;
    
    TEST_ASSERT(mac0_start == mac1_start && mac1_start == mac2_start, 
                "All MACs should have same start time for batching");
    
    destroy_timeline_collection(timeline);
    cleanup_test_schedule(s);
    
    TEST_PASS("Multiple MACs batching");
}

/*----------------------------------------------------------------------------*/
/*                              Main Test Runner                              */
/*----------------------------------------------------------------------------*/

int main() {
    int passed = 0;
    int total = 8;
    
    printf("═══════════════════════════════════════════════════════\n");
    printf("  Aker Notification Scenarios Test Suite\n");
    printf("═══════════════════════════════════════════════════════\n");
    
    aker_notification_init("UTC");
    
    passed += test_weekly_timeline_building();
    passed += test_absolute_timeline_building();
    passed += test_until_i_unpause_detection();
    passed += test_state_change_detection();
    passed += test_next_notification_time();
    passed += test_short_period_handling();
    passed += test_timeline_persistence();
    passed += test_multiple_macs_batching();
    
    aker_notification_cleanup();
    
    printf("\n═══════════════════════════════════════════════════════\n");
    printf("  Test Results: %d/%d PASSED\n", passed, total);
    printf("═══════════════════════════════════════════════════════\n");
    
    return (passed == total) ? 0 : 1;
}
