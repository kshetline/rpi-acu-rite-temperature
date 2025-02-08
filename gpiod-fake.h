#ifndef GPIOD_FAKE
#define GPIOD_FAKE

#include <stdint.h>
#include <time.h>

#define GPIOD_CTXLESS_EVENT_RISING_EDGE  1
#define GPIOD_CTXLESS_EVENT_FALLING_EDGE 2
#define GPIOD_CTXLESS_EVENT_BOTH_EDGES   3

#define GPIOD_CTXLESS_EVENT_CB_RISING_EDGE  2
#define GPIOD_CTXLESS_EVENT_CB_FALLING_EDGE 3

struct gpiod_ctxless_event_poll_fd {
  int fd;
  bool event;
};

typedef int (*gpiod_ctxless_event_handle_cb)(int, unsigned int,
    const struct timespec *, void *);

typedef int (*gpiod_ctxless_event_poll_cb)(unsigned int,
  struct gpiod_ctxless_event_poll_fd *,
  const struct timespec *, void *);

int gpiod_ctxless_event_monitor(const char* device, int event_type, unsigned int dataPin, bool active_low,
      const char* consumer, const timespec* timeout, gpiod_ctxless_event_poll_cb poll_cb,
      gpiod_ctxless_event_handle_cb event_cb, void* miscData);

void fakeGpiodInit();

#endif
