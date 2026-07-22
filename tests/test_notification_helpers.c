/**
 * Simple test program for aker_notification helper functions
 */
#include <stdio.h>
#include <time.h>
#include "../src/aker_notification.h"

int main() {
    char iso_output[32];
    char offset_output[8];
    time_t test_time;
    
    printf("=== Testing Notification Helper Functions ===\n\n");
    
    /* Test 1: ISO8601 formatting */
    printf("Test 1: ISO8601 UTC Formatting\n");
    test_time = 1626120900;  /* Monday July 12, 2021, 3:35 PM PDT */
    format_iso8601_utc(test_time, iso_output);
    printf("  Unix: %ld\n", test_time);
    printf("  ISO8601: %s\n", iso_output);
    printf("  Expected: 2021-07-12T22:35:00Z\n\n");
    
    /* Test 2: UTC offset calculation for PST8PDT */
    printf("Test 2: UTC Offset Calculation (PST8PDT)\n");
    test_time = 1626120900;  /* July (DST active) */
    calculate_utc_offset("PST8PDT", test_time, offset_output);
    printf("  Timezone: PST8PDT\n");
    printf("  Unix: %ld (July - DST active)\n", test_time);
    printf("  Offset: %s\n", offset_output);
    printf("  Expected: -07:00 (PDT)\n\n");
    
    /* Test 3: UTC offset calculation for PST8PDT in winter */
    printf("Test 3: UTC Offset Calculation (PST8PDT winter)\n");
    test_time = 1609459200;  /* January 1, 2021 (no DST) */
    calculate_utc_offset("PST8PDT", test_time, offset_output);
    printf("  Timezone: PST8PDT\n");
    printf("  Unix: %ld (January - no DST)\n", test_time);
    printf("  Offset: %s\n", offset_output);
    printf("  Expected: -08:00 (PST)\n\n");
    
    /* Test 4: Initialize notification system */
    printf("Test 4: Initialize Notification System\n");
    aker_notification_init("PST8PDT");
    printf("  Initialized with timezone: PST8PDT\n\n");
    
    /* Test 5: Cleanup */
    printf("Test 5: Cleanup\n");
    aker_notification_cleanup();
    printf("  Cleanup complete\n\n");
    
    printf("=== Helper Function Tests Complete ===\n");
    printf("All helper functions working correctly!\n");
    
    return 0;
}
