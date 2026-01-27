module AccelerometerC {
    uses interface Boot;
    uses interface Leds;
    uses interface Timer<TMilli> as TimerAccel;
    uses interface Read<uint16_t> as Zaxis;
    uses interface Read<uint16_t> as Yaxis;
    uses interface Read<uint16_t> as Xaxis;
    uses interface SplitControl as AccelControl;
}
implementation {
    int16_t x_threshold = 50;
    int16_t y_threshold = 50;
    int16_t z_threshold = 50;
    
    event void Boot.booted() {
        call AccelControl.start();
    }

    event void TimerAccel.fired() {
        call Xaxis.read();
    }

    event void AccelControl.startDone(error_t err) {
        call TimerAccel.startPeriodic(1);
    }

    event void AccelControl.stopDone(error_t err) {
    }

    event void Xaxis.readDone(error_t result, uint16_t data) {
        if (abs(data) > x_threshold) {
            call Leds.led0On();
        } else {
            call Leds.led0Off();
        }
        call Yaxis.read();
    }

    event void Yaxis.readDone(error_t result, uint16_t data) {
        if (abs(data) > y_threshold) {
            call Leds.led1On();
        } else {
            call Leds.led1Off();
        }
        call Zaxis.read();
    }

    event void Zaxis.readDone(error_t result, uint16_t data) {
        if (abs(data) > z_threshold) {
            call Leds.led2On();
        } else {
            call Leds.led2Off();
        }
    }
}
