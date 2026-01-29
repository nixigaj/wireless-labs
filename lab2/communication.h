#pragma once
#include <sys/types.h>

typedef enum EventType {
  BUTTON = 0,
  ACCEL = 1,
} event_type_t;

typedef enum AccelAxis {
  X = 0,
  Y = 1,
  Z = 2,
} accel_axis_t;

typedef struct PayloadEvent {
  event_type_t type;
  union {
    struct {
      int button_id;
    } button;

    struct {
      accel_axis_t axis;
      float value;
    } accel;
  } data;
} payload_event_t;
