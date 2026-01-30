#include "communication.h"
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
#define ACCM_READ_INTERVAL CLOCK_SECOND / 10

static process_event_t button_event = 1;
static process_event_t accelerator_event = 2;

/*---------------------------------------------------------------------------*/
PROCESS(accel_process, "Test Accel process");
PROCESS(button_process, "Test Accel process");
PROCESS(comm_process, "Test Accel process");
AUTOSTART_PROCESSES(&comm_process, &accel_process, &button_process);

/*---------------------------------------------------------------------------*/
/* Callback function for received packets.
 *
 * Whenever this node receives a packet for its broadcast handle,
 * this function will be called.
 */
static void recv(const void *data, uint16_t len, const linkaddr_t *src,
                 const linkaddr_t *dest) {}

inline static void send() { NETSTACK_NETWORK.output(NULL); }

/* Main process, setups  */

static struct etimer et;
PROCESS_THREAD(accel_process, ev, data) {

  PROCESS_BEGIN();
  static payload_event_t payload = {.type = ACCEL};
  static int16_t x = 0;
  accm_init();

  while (1) {
    etimer_set(&et, ACCM_READ_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    x = accm_read_axis(X_AXIS);
    if (abs(x) > 200) {
      printf("accelerator\n");
      process_post(&comm_process, accelerator_event, &payload);
    }
  }
  PROCESS_END();
}

PROCESS_THREAD(button_process, ev, data) {
  PROCESS_BEGIN();
  static payload_event_t payload = {.type = BUTTON};
  SENSORS_ACTIVATE(button_sensor);

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);
    process_post(&comm_process, button_event, &payload);
  }
  PROCESS_END();
}

PROCESS_THREAD(comm_process, ev, data) {
  static payload_event_t event = {.type = BUTTON};
  PROCESS_BEGIN();
  /* Initialize NullNet */
  nullnet_buf = (uint8_t *)&event;
  nullnet_len = sizeof(event);
  nullnet_set_input_callback(recv);

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == button_event || ev == accelerator_event);
    if (ev == button_event) {
      printf("button pressed;\n");
      memcpy(nullnet_buf, data, sizeof(event));
      nullnet_len = sizeof(event);
      send();
    } else if (ev == accelerator_event) {
      printf("accelerometer found\n");
      memcpy(nullnet_buf, data, sizeof(event));
      nullnet_len = sizeof(event);
      send();
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
