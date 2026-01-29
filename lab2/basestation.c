#include "communication.h"
#include "contiki.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/ctimer.h"
#include <stdio.h>
#include <sys/types.h>

#define INTRUDER_TIMEOUT CLOCK_SECOND * 2

#define BUTTON_LED 0b0001
#define ACCEL_LED 0b0010

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
static struct etimer timer;
static struct etimer button_timer;
static struct etimer accel_timer;

static bool button_active = false;
static bool accel_active = false;
enum station_event_t {
  button_pressed,
  accel_detected,
  button_timer_expired,
  accel_timer_expired,
  alarm_timer_expired,
};

typedef void (*fn_t)(void);
typedef struct State {
  uint8_t id;
  fn_t Enter;
  fn_t Do;
  fn_t Exit;
} state_t;

static void alarm_enter() {}
static void alarm_do() {}
static void alarm_exit() {}

static void button_enter() {}
static void button_do() {}
static void button_exit() {}

static void accel_enter() {}
static void accel_do() {}
static void accel_exit() {}

static state_t alarm = {0, alarm_enter, alarm_do, alarm_exit};
static state_t button = {1, button_enter, button_do, button_exit};
static state_t accel = {2, accel_enter, accel_do, accel_exit};

static void recv(const void *data, uint16_t len, const linkaddr_t *src,
                 const linkaddr_t *dest) {
  static payload_event_t *payload;
  *payload = *(payload_event_t *)data;
  switch (payload->type) {
  case button_pressed:
    process_post(&led_process, button_pressed, &payload);
    break;
  case accel_detected:
    process_post(&led_process, accel_detected, &payload);
    break;
  default:
    printf("received invalid event.");
    break;
  }
}

PROCESS_THREAD(timer_process, ev, data) {
  static station_event_t event;
  PROCESS_START();
  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(PROCESS_EVENT_TIMER);
    switch (data) {
    case &button_timer:
      event = button_timer_expired;
      break;
    case &accel_timer:
      event = accel_timer_expired;
      break;
    case &timer:
      break;
    default:
      printf("invalid timer fired.");
      continue;
      break;
    }
    process_post(led_process, event, data);
  }

  PROCESS_END();
}

PROCESS_THREAD(led_process, ev, data) {
  PROCESS_BEGIN();
  static payload_event_t *payload;

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == button_pressed || ev == accel_detected ||
                             ev == button_timer_expired ||
                             ev == accel_timer_expired ||
                             ev == alarm_timer_expired);
    if (ev == PROCESS_EVENT_POLL) {
      *payload = *(payload_event_t *)data;
      switch (payload->type) {
      case BUTTON:
        button_active = true;
        leds_on(BUTTON_LED);
        break;
      case ACCEL:
        accel_active = true;
        break;
      default:
        printf("received invalid event type.\n");
        break;
      }
      if (button_active && accel_active) {
        etimer_set(&timer, INTRUDER_TIMEOUT);
        etimer_set(&button_timer, INTRUDER_TIMEOUT);
        etimer_set(&button_timer, INTRUDER_TIMEOUT);
      }
      leds_on(0b1111);
    } else if (etimer_expired(&timer)) {
      leds_off(0b1111);
    } else if (etimer_expired(&button_timer)) {
      button_active = false;
      leds_off(BUTTON_LED);
    } else if (etimer_expired(&accel_timer)) {
      accel_active = false;
      leds_off(ACCEL_LED);
    }
  }
  PROCESS_END();
}

/* Our main process. */
PROCESS_THREAD(basestation_process, ev, data) {
  PROCESS_BEGIN();

  /* Initialize NullNet */
  nullnet_set_input_callback(recv);

  PROCESS_END();
}
