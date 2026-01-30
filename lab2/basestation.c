#include "communication.h"
#include "contiki.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/ctimer.h"
#include <stdio.h>
#include <sys/types.h>

#define ALARM_TIMEOUT CLOCK_SECOND * 4
#define BUTTON_TIMEOUT CLOCK_SECOND * 4
#define ACCEL_TIMEOUT CLOCK_SECOND * 4
#define BUTTON_LED 0b0001
#define ACCEL_LED 0b0010
#define ALL_LEDS 0b1111

/* Declare our "main" process, the basestation_process */
PROCESS(basestation_process, "Intruder basestation");
PROCESS(timer_process, "Timer handling process");
PROCESS(event_process, "Event handling process");
/* The basestation process should be started automatically when
 * the node has booted. */
AUTOSTART_PROCESSES(&event_process, &basestation_process, &timer_process);

typedef enum {
  STATE_INACTIVE,
  STATE_BUTTON,
  STATE_ACCEL,
  STATE_ALARM,
  STATE_NONE
} state_id_t;
static const char *state_strings[5] = {"inactive", "button", "accel", "alarm",
                                 "none"};

typedef enum {
  EV_BUTTON_PRESSED = 0,
  EV_ACCEL_DETECTED = 1,
  EV_BUTTON_TIMER_EXPIRED = 2,
  EV_ACCEL_TIMER_EXPIRED = 3,
  EV_ALARM_TIMER_EXPIRED = 4,
  EV_SET_BUTTON_TIMER = 5,
  EV_SET_ACCEL_TIMER = 6,
  EV_SET_ALARM_TIMER = 7,
} state_event_t;
static const char *ev_strings[8] = {
  "BUTTON_PRESSED",
  "ACCEL_DETECTED",
  "BUTTON_TIMER_EXPIRED",
  "ACCEL_TIMER_EXPIRED",
  "ALARM_TIMER_EXPIRED",
  "SET_BUTTON_TIMER",
  "SET_ACCEL_TIMER",
  "SET_ALARM_TIMER",
};

static process_event_t events[8] = {
    EV_BUTTON_PRESSED,      EV_ACCEL_DETECTED,      EV_BUTTON_TIMER_EXPIRED,
    EV_ACCEL_TIMER_EXPIRED, EV_ALARM_TIMER_EXPIRED, EV_SET_BUTTON_TIMER,
    EV_SET_ACCEL_TIMER,     EV_SET_ALARM_TIMER,
};

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

static void set_alarm_timer() {
  /* etimer_set(&alarm_timer, ALARM_TIMEOUT); */
  process_post(&timer_process, events[EV_SET_ALARM_TIMER], NULL);
  printf("start alarm timer\n");
}
static void try_set_alarm_timer() {
  if (!etimer_expired(&button_timer) && !etimer_expired(&accel_timer)) {
    process_post(&timer_process, events[EV_SET_ALARM_TIMER], NULL);
  }
  /* etimer_set(&alarm_timer, ALARM_TIMEOUT); */
  printf("reset alarm timer\n");
}
static void set_button_timer() {
  process_post(&timer_process, events[EV_SET_BUTTON_TIMER], NULL);
  /* etimer_set(&button_timer, BUTTON_TIMEOUT); */
  printf("start button timer\n");
}
static void set_accel_timer() {
  process_post(&timer_process, events[EV_SET_ACCEL_TIMER], NULL);
  /* etimer_set(&accel_timer, ACCEL_TIMEOUT); */
  printf("start accel timer\n");
}
static void turn_off_leds() {
  leds_off(ALL_LEDS);
  printf("turn off leds\n");
}
static void turn_on_alarm_leds() {
  leds_on(ALL_LEDS);
  printf("turn on alarm leds\n");
}
static void turn_on_button_leds() {
  leds_on(BUTTON_LED);
  printf("turn on button leds\n");
}
static void turn_on_accel_leds() {
  leds_on(ACCEL_LED);
  printf("turn on accel leds\n");
}

// NOTE: assuming alarm timer is longer or equal to button and accel timers
static void handle_alarm_timer_expired() {
  printf("alarm timer expired\n");
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

static transition transition_table[4][5] = {
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

        {STATE_ACCEL, {set_accel_timer, turn_on_accel_leds, NULL}},
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
  printf("current state: %s\n", state_strings[previous_state]);
  printf("next state: %s\n", state_strings[current_state]);
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
  printf("data_received\n");
  process_post(&event_process, events[local_event], NULL);
}

PROCESS_THREAD(timer_process, ev, data) {
  /* static state_event_t local_event; */
  PROCESS_BEGIN();
  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(
        ev == events[EV_SET_BUTTON_TIMER] || ev == events[EV_SET_ACCEL_TIMER] ||
        ev == events[EV_SET_ALARM_TIMER] || ev == PROCESS_EVENT_TIMER);
    if (ev == PROCESS_EVENT_TIMER) {
      printf("timeout from timer\n");
      if (data == &button_timer) {
        process_post(&event_process, events[EV_BUTTON_TIMER_EXPIRED], NULL);
      } else if (data == &accel_timer) {
        process_post(&event_process, events[EV_ACCEL_TIMER_EXPIRED], NULL);
      } else if (data == &alarm_timer) {
        process_post(&event_process, events[EV_ALARM_TIMER_EXPIRED], NULL);
      }
    } else if (ev == events[EV_SET_BUTTON_TIMER]) {
      etimer_set(&button_timer, BUTTON_TIMEOUT);
    } else if (ev == events[EV_SET_ACCEL_TIMER]) {
      etimer_set(&accel_timer, ACCEL_TIMEOUT);
    } else if (ev == events[EV_SET_ALARM_TIMER]) {
      etimer_set(&alarm_timer, ALARM_TIMEOUT);
    } else {
      printf("invalid timer fired.");
    }
  }

  PROCESS_END();
}

PROCESS_THREAD(event_process, ev, data) {
  PROCESS_BEGIN();
  while (1) {
    printf("-------------------------\n");
    PROCESS_WAIT_EVENT_UNTIL(ev == events[EV_BUTTON_PRESSED] ||
                             ev == events[EV_ACCEL_DETECTED] ||
                             ev == events[EV_BUTTON_TIMER_EXPIRED] ||
                             ev == events[EV_ACCEL_TIMER_EXPIRED] ||
                             ev == events[EV_ALARM_TIMER_EXPIRED]);
    printf("got event %s\n",  ev_strings[ev]);
    transfer_state((state_event_t)ev);
  }
  PROCESS_END();
}

/* Our main process. */
PROCESS_THREAD(basestation_process, ev, data) {
  PROCESS_BEGIN();
  nullnet_set_input_callback(recv);

  while (1) {
    PROCESS_YIELD();
  }

  PROCESS_END();
}
