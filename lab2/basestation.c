#include "communication.h"
#include "contiki.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/ctimer.h"
#include <stdio.h>
#include <sys/types.h>

#define ALARM_TIMEOUT CLOCK_SECOND * 2
#define BUTTON_TIMEOUT CLOCK_SECOND
#define ACCEL_TIMEOUT CLOCK_SECOND
#define BUTTON_LED 0b0001
#define ACCEL_LED 0b0010
#define ALL_LEDS 0b1111

/* Declare our "main" process, the basestation_process */
PROCESS(basestation_process, "Intruder basestation");
PROCESS(timer_process, "Timer handling process");
PROCESS(event_process, "Event handling process");
/* The basestation process should be started automatically when
 * the node has booted. */
AUTOSTART_PROCESSES(&basestation_process, &event_process, &timer_process);

/* Callback function for received packets.
 *
 * Whenever this node receives a packet for its broadcast handle,
 * this function will be called.
 */
/*static void timeout_reached_cb(void *ptr) { leds_off(0b1110); }*/

typedef enum {
  STATE_INACTIVE,
  STATE_BUTTON,
  STATE_ACCEL,
  STATE_ALARM,
  STATE_NONE
} state_id_t;

typedef enum {
  EV_BUTTON_PRESSED,
  EV_ACCEL_DETECTED,
  EV_BUTTON_TIMER_EXPIRED,
  EV_ACCEL_TIMER_EXPIRED,
  EV_ALARM_TIMER_EXPIRED
} state_event_t;

static state_id_t previous_state = STATE_INACTIVE;
static state_id_t current_state = STATE_INACTIVE;
static struct etimer alarm_timer;
static struct etimer button_timer;
static struct etimer accel_timer;

typedef void (*action_fn_t)(void);
typedef struct {
  state_id_t next_state;
  action_fn_t actions[5]; // MUST END WITH NULL
} transition;
/* typedef struct State { */
/*   uint8_t id; */
/*   fn_t Enter; */
/*   fn_t Exit; */
/* } state_t; */

static void set_alarm_timer() { etimer_set(&alarm_timer, ALARM_TIMEOUT); }
static void try_set_alarm_timer() {
  if (!etimer_expired(&button_timer) && !etimer_expired(&accel_timer))
    etimer_set(&alarm_timer, ALARM_TIMEOUT);
}
static void set_button_timer() { etimer_set(&button_timer, BUTTON_TIMEOUT); }
static void set_accel_timer() { etimer_set(&accel_timer, ACCEL_TIMEOUT); }
static void turn_off_leds() { leds_off(ALL_LEDS); }
static void turn_on_alarm_leds() { leds_on(ALL_LEDS); }
static void turn_on_button_leds() { leds_on(BUTTON_LED); }
static void turn_on_accel_leds() { leds_on(ACCEL_LED); }

// NOTE: assuming alarm timer is longer or equal to button and accel timers
static void handle_alarm_timer_expired() {
  turn_off_leds();
  if (!etimer_expired(&button_timer)) {
    turn_on_button_leds();
    current_state = STATE_BUTTON;
  } else if (!etimer_expired(&accel_timer)) {
    turn_on_accel_leds();
    current_state = STATE_ACCEL;
  } else {
    current_state = STATE_INACTIVE;
  }
}


transition transition_table[4][5] = {
    //   EV_BUTTON_PRESSED,  EV_ACCEL_DETECTED,  EV_BUTTON_TIMER_EXPIRED,
    //   EV_ACCEL_TIMER_EXPIRED,  EV_ALARM_TIMER_EXPIRED
    /*INACTIVE*/ {
        {STATE_BUTTON, {set_button_timer, turn_on_button_leds, NULL}},
        {STATE_ACCEL, {set_accel_timer, turn_on_accel_leds, NULL}},
        {STATE_NONE, {NULL}},
        {STATE_NONE, {NULL}},
        {STATE_NONE, {NULL}},
    },
    /*BUTTON*/
    {
        {STATE_BUTTON, {set_button_timer, NULL}},
        {STATE_ALARM, {set_alarm_timer, turn_on_alarm_leds, NULL}},
        {STATE_INACTIVE, {turn_off_leds, NULL}},
        {STATE_NONE, {NULL}},
        {STATE_NONE, {NULL}},
    },
    /*ACCEL*/
    {
        {STATE_ALARM, {set_alarm_timer, turn_on_alarm_leds, NULL}},
        {STATE_ACCEL, {set_accel_timer, NULL}},
        {STATE_NONE, {NULL}},
        {STATE_INACTIVE, {turn_off_leds, NULL}},
        {STATE_NONE, {NULL}},
    },
    /*ALARM*/
    {
        {STATE_ALARM, {set_button_timer, try_set_alarm_timer, NULL}},
        {STATE_ALARM, {set_accel_timer, try_set_alarm_timer, NULL}},
        {STATE_ALARM, {NULL}},
        {STATE_ALARM, {NULL}},
        {STATE_ALARM, {handle_alarm_timer_expired, NULL}},
    },
};

static void apply_actions(action_fn_t *actions) {
  while (*actions) {
    (*actions++)();
  }
}
static void transfer_state(state_event_t evt) {
  previous_state = current_state;
  current_state = transition_table[previous_state][evt].next_state;
  apply_actions(transition_table[previous_state][evt].actions);
}

static void recv(const void *data, uint16_t len, const linkaddr_t *src,
                 const linkaddr_t *dest) {
  static state_event_t local_event;
  static payload_event_t payload;
  payload = *(payload_event_t *)data;
  switch (payload.type) {
  case BUTTON:
    local_event = EV_BUTTON_PRESSED;
    break;
  case ACCEL:
    local_event = EV_ACCEL_DETECTED;
    break;
  default:
    printf("received invalid event.");
    return;
    break;
  }
  process_post(&event_process, local_event, NULL);
}

PROCESS_THREAD(timer_process, ev, data) {
  PROCESS_BEGIN();
  static state_event_t local_event;
  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(PROCESS_EVENT_TIMER);
    if (data == &button_timer) {
      local_event = EV_BUTTON_TIMER_EXPIRED;
    } else if (data == &accel_timer) {
      local_event = EV_ACCEL_TIMER_EXPIRED;
    } else if (data == &alarm_timer) {
      local_event = EV_ALARM_TIMER_EXPIRED;
    } else {
      printf("invalid timer fired.");
      continue;
    }
    process_post(&event_process, local_event, NULL);
  }

  PROCESS_END();
}

PROCESS_THREAD(event_process, ev, data) {
  PROCESS_BEGIN();
  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(
        ev == EV_BUTTON_PRESSED || ev == EV_ACCEL_DETECTED ||
        ev == EV_BUTTON_TIMER_EXPIRED || ev == EV_ACCEL_TIMER_EXPIRED ||
        ev == EV_ALARM_TIMER_EXPIRED);
    transfer_state((state_event_t)ev);
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
