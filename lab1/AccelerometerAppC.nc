configuration AccelerometerAppC {}

implementation {
    components MainC, AccelerometerC as App, LedsC, new TimerMilliC(), new ADXL345C();
    App.Boot -> MainC;
    App.TimerAccel -> TimerMilliC;
    App.Leds -> LedsC;

    App.Zaxis -> ADXL345C.Z;
    App.Yaxis -> ADXL345C.Y;
    App.Xaxis -> ADXL345C.X;
    App.AccelControl -> ADXL345C.SplitControl;
}
