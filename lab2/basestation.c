#include "contiki.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/ctimer.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define INTRUDER_TIMEOUT CLOCK_SECOND * 2

/* Declare our "main" process, the basestation_process */
PROCESS(basestation_process, "Intruder basestation");
PROCESS(led_process, "LED handling process");
/* The basestation process should be started automatically when
 * the node has booted. */
AUTOSTART_PROCESSES(&basestation_process, &led_process);

/* Callback function for received packets.
 *
 * Whenever this node receives a packet for its broadcast handle,
 * this function will be called.
 */
/*static void timeout_reached_cb(void *ptr) { leds_off(0b1110); }*/

static void recv(const void *data, uint16_t len, const linkaddr_t *src,
                 const linkaddr_t *dest) {
  /*printf("received something\n");*/
  process_post(&led_process, PROCESS_EVENT_POLL, NULL);
}

static struct etimer timer;
PROCESS_THREAD(led_process, ev, data) {
  PROCESS_BEGIN();

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL || ev == PROCESS_EVENT_TIMER);
    if (ev == PROCESS_EVENT_POLL) {
        etimer_set(&timer, INTRUDER_TIMEOUT);
        leds_on(0b1111);
    } else if (etimer_expired(&timer)){
        leds_off(0b1111);
    }
  }
  PROCESS_END();
}

/* Our main process. */
PROCESS_THREAD(basestation_process, ev, data) {
  PROCESS_BEGIN();

  /* Initialize NullNet */
  nullnet_set_input_callback(recv);

  /*while (1) {*/
  /*  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&blinkTimer));*/
  /*  printf("running\n");*/
  /*  leds_toggle(0b0001);*/
  /*}*/

  PROCESS_END();
}
