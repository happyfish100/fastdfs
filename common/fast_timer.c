#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "logger.h"
#include "fast_timer.h"

int fast_timer_init(FastTimer *timer, const int slot_count,
    const int64_t current_time)
{
  int bytes;
  if (slot_count <= 0 || current_time <= 0) {
    return EINVAL;
  }

  timer->slot_count = slot_count;
  timer->base_time = current_time; //base time for slot 0
  timer->current_time = current_time;
  bytes = sizeof(FastTimerSlot) * slot_count;
  timer->slots = (FastTimerSlot *)malloc(bytes);
  if (timer->slots == NULL) {
     return errno != 0 ? errno : ENOMEM;
  }
  memset(timer->slots, 0, bytes);
  return 0;
}

void fast_timer_destroy(FastTimer *timer)
{
  if (timer->slots != NULL) {
    free(timer->slots);
    timer->slots = NULL;
  }
}

#define TIMER_GET_SLOT_INDEX(timer, expires) \
  (((expires) - timer->base_time) % timer->slot_count)

#define TIMER_GET_SLOT_POINTER(timer, expires) \
  (timer->slots + TIMER_GET_SLOT_INDEX(timer, expires))

int fast_timer_add(FastTimer *timer, FastTimerEntry *entry)
{
  FastTimerSlot *slot;

  slot = TIMER_GET_SLOT_POINTER(timer, entry->expires >
     timer->current_time ? entry->expires : timer->current_time);
  entry->next = slot->head.next;
  if (slot->head.next != NULL) {
    slot->head.next->prev = entry;
  }
  entry->prev = &slot->head;
  slot->head.next = entry;
  return 0;
}

int fast_timer_modify(FastTimer *timer, FastTimerEntry *entry,
    const int64_t new_expires)
{
  if (new_expires == entry->expires) {
    return 0;
  }

  if (new_expires < entry->expires) {
    fast_timer_remove(timer, entry);
    entry->expires = new_expires;
    return fast_timer_add(timer, entry);
  }

  entry->rehash = TIMER_GET_SLOT_INDEX(timer, new_expires) !=
      TIMER_GET_SLOT_INDEX(timer, entry->expires);
  entry->expires = new_expires;  //lazy move
  return 0;
}

int fast_timer_remove(FastTimer *timer, FastTimerEntry *entry)
{
  if (entry->prev == NULL) {
     return ENOENT;   //already removed
  }

  if (entry->next != NULL) {
     entry->next->prev = entry->prev;
     entry->prev->next = entry->next;
     entry->next = NULL;
  }
  else {
     entry->prev->next = NULL;
  }

  entry->prev = NULL;
  return 0;
}

FastTimerSlot *fast_timer_slot_get(FastTimer *timer, const int64_t current_time)
{
  if (timer->current_time >= current_time) {
    return NULL;
  }

  return TIMER_GET_SLOT_POINTER(timer, timer->current_time++);
}

int fast_timer_timeouts_get(FastTimer *timer, const int64_t current_time,
   FastTimerEntry *head)
{
  FastTimerSlot *slot;
  FastTimerEntry *entry;
  FastTimerEntry *first;
  FastTimerEntry *last;
  FastTimerEntry *tail;
  int count;

  head->prev = NULL;
  head->next = NULL;
  if (timer->current_time >= current_time) {
    return 0;
  }

  first = NULL;
  last = NULL;
  tail = head;
  count = 0;
  while (timer->current_time < current_time) {
    slot = TIMER_GET_SLOT_POINTER(timer, timer->current_time++);
    entry = slot->head.next;
    while (entry != NULL) {
      if (entry->expires >= current_time) {  //not expired
         if (first != NULL) {
            first->prev->next = entry;
            entry->prev = first->prev;

            tail->next = first;
            first->prev = tail;
            tail = last;
            first = NULL;
         }
         if (entry->rehash) {
           last = entry;
           entry = entry->next;

           last->rehash = false;
           fast_timer_remove(timer, last);
           fast_timer_add(timer, last);
           continue;
         }
      }
      else {
         count++;
         if (first == NULL) {
            first = entry;
         }
      }
      last = entry;
      entry = entry->next;
    }

    if (first != NULL) {
       first->prev->next = NULL;

       tail->next = first;
       first->prev = tail;
       tail = last;
       first = NULL;
    }
  }

  if (count > 0) {
     tail->next = NULL;
  }

  return count;
}

