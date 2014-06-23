#ifndef __FAST_TIMER_H__
#define __FAST_TIMER_H__

#include <stdint.h>
#include "common_define.h"

typedef struct fast_timer_entry {
  int64_t expires;
  void *data;
  struct fast_timer_entry *prev;
  struct fast_timer_entry *next;
  bool rehash;
} FastTimerEntry;

typedef struct fast_timer_slot {
  struct fast_timer_entry head;
} FastTimerSlot;

typedef struct fast_timer {
  int slot_count;    //time wheel slot count
  int64_t base_time; //base time for slot 0
  int64_t current_time;
  FastTimerSlot *slots;
} FastTimer;

#ifdef __cplusplus
extern "C" {
#endif

int fast_timer_init(FastTimer *timer, const int slot_count,
    const int64_t current_time);
void fast_timer_destroy(FastTimer *timer);

int fast_timer_add(FastTimer *timer, FastTimerEntry *entry);
int fast_timer_remove(FastTimer *timer, FastTimerEntry *entry);
int fast_timer_modify(FastTimer *timer, FastTimerEntry *entry,
    const int64_t new_expires);

FastTimerSlot *fast_timer_slot_get(FastTimer *timer, const int64_t current_time);
int fast_timer_timeouts_get(FastTimer *timer, const int64_t current_time,
   FastTimerEntry *head);

#ifdef __cplusplus
}
#endif

#endif

