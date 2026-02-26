#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include <stdio.h>
#include <string.h>

struct event {
  clock_time_t time;
  linkaddr_t addr;
};

enum Events {
  START_TIMER,
};

#define MAX_NUMBER_OF_EVENTS 3
struct event event_history[MAX_NUMBER_OF_EVENTS] = {
    {0, {{0}}}, {0, {{0}}}, {0, {{0}}}};

/*---------------------------------------------------------------------------*/

PROCESS(clicker_process, "Clicker NG Process");
PROCESS(timer_process, "Timer Process");
AUTOSTART_PROCESSES(&clicker_process, &timer_process);

/*---------------------------------------------------------------------------*/


static void handle_event(const linkaddr_t *src) {
  static int i = 0;
  static int event_idx = 0;
  static int nr_active = 0;
  static clock_time_t curr_time;
  curr_time = clock_time();
  for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
    if (event_history[i].time == 0) {
      event_idx = i;
      break;
    } else if (linkaddr_cmp(&(event_history[event_idx].addr), src)) {
      event_idx = i;
      break;
    } else if (event_history[event_idx].time > event_history[i].time) {
      event_idx = i;
    }
  }
  event_history[event_idx] = (struct event){.time = clock_time(), .addr = *src};

  nr_active = 0;
  for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
    if (event_history[i].time == 0) {
      continue;
    }

    if (curr_time - event_history[i].time < CLOCK_SECOND * 10) {
      nr_active++;
    }
  }
  if (nr_active == MAX_NUMBER_OF_EVENTS) {
    printf("Start timer on  %d\n", linkaddr_node_addr.u8[0]);
    process_post(&timer_process, START_TIMER, NULL);
  }
}

/*---------------------------------------------------------------------------*/

static void recv(const void *data, uint16_t len, const linkaddr_t *src,
                 const linkaddr_t *dest) {
  printf("Received: %s - from %d\n", (char *)data, src->u8[0]);
  leds_toggle(LEDS_GREEN);
  handle_event(src);
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/

PROCESS_THREAD(clicker_process, ev, data) {
  static char payload[] = "alarm";

  PROCESS_BEGIN();
  /* Initialize NullNet */
  nullnet_buf = (uint8_t *)&payload;
  nullnet_len = sizeof(payload);
  nullnet_set_input_callback(recv);

  /* Activate the button sensor. */
  SENSORS_ACTIVATE(button_sensor);

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);

    printf("event: button klick on %d\n", linkaddr_node_addr.u8[0]);

    memcpy(nullnet_buf, &payload, sizeof(payload));
    nullnet_len = sizeof(payload);

    /* Send the content of the packet buffer using the
     * broadcast handle. */
    NETSTACK_NETWORK.output(NULL);
    handle_event(&linkaddr_node_addr);
  }

  PROCESS_END();
}

static struct etimer timer_on;
PROCESS_THREAD(timer_process, ev, data) {
  PROCESS_BEGIN();
  clock_init();

  /* Setup Timer */
  etimer_set(&timer_on, CLOCK_SECOND * 3);
  etimer_stop(&timer_on);

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == START_TIMER || ev == PROCESS_EVENT_TIMER);
    if (ev == START_TIMER) {
      leds_set(LEDS_YELLOW);
      etimer_restart(&timer_on);
    } else if (ev == PROCESS_EVENT_TIMER) {
      leds_set(0);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
