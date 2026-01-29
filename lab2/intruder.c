#include "contiki.h"
#include "dev/adxl345.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LED_INT_ONTIME CLOCK_SECOND / 2
#define ACCM_READ_INTERVAL CLOCK_SECOND / 100

static process_event_t ledOff_event;
/*---------------------------------------------------------------------------*/
PROCESS(accel_process, "Test Accel process");
PROCESS(led_process, "LED handling process");
AUTOSTART_PROCESSES(&accel_process, &led_process); 

/*---------------------------------------------------------------------------*/
/* Callback function for received packets.
 *
 * Whenever this node receives a packet for its broadcast handle,
 * this function will be called.
 */
static void recv(const void *data, uint16_t len, const linkaddr_t *src,
                 const linkaddr_t *dest) {}

/*---------------------------------------------------------------------------*/
/* When posted an ledOff event, the LEDs will switch off after LED_INT_ONTIME.
      static process_event_t ledOff_event;
      ledOff_event = process_alloc_event();
      process_post(&led_process, ledOff_event, NULL);
*/
static struct etimer ledETimer;
PROCESS_THREAD(led_process, ev, data) {
  PROCESS_BEGIN();
  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == ledOff_event);
    etimer_set(&ledETimer, LED_INT_ONTIME);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&ledETimer));
    leds_off(LEDS_RED + LEDS_GREEN);
  }
  PROCESS_END();
}

/* Main process, setups  */

static struct etimer et;

PROCESS_THREAD(accel_process, ev, data) {
  static char payload[] = "Intruder!";

  PROCESS_BEGIN();
  int16_t x;

  /* Register the event used for lighting up an LED when interrupt strikes. */
  ledOff_event = process_alloc_event();

  /* Initialize NullNet */
  nullnet_buf = (uint8_t *)&payload;
  nullnet_len = sizeof(payload);
  nullnet_set_input_callback(recv);

  /* Start and setup the accelerometer with default values, eg no interrupts
   * enabled. */
  accm_init();

  while (1) {
    etimer_set(&et, ACCM_READ_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    x = accm_read_axis(X_AXIS);
    if (abs(x) > 150) {
      nullnet_buf = (uint8_t *)&payload;
      nullnet_len = sizeof(payload);
      memcpy(nullnet_buf, &payload, sizeof(payload));
      nullnet_len = sizeof(payload);
      NETSTACK_NETWORK.output(NULL);
    }
  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
