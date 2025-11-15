#include <Triac.h>

// Outside of class
Triac *pointerToTriac;          // declare a pointer to testLib class

static void triacInterruptHandler(void) {           // define global handler
  pointerToTriac->currentNull();                    // calls class member handler
}

static void triacOnTimerHandler(void) {           // define global handler
  pointerToTriac->onTimer();                    // calls class member handler
}


    Triac::Triac(int user_dimmer_pin, int zc_dimmer_pin){
            dimmer_pin = user_dimmer_pin;
            zc_pin = zc_dimmer_pin;
        if(debug){
            Serial.println("  in Triac construtor ");
        }
        pinMode(dimmer_pin, OUTPUT);
    	pinMode(zc_pin, INPUT_PULLUP);
    }

    Triac::~Triac(void)
      {};

    void Triac::begin(ON_OFF_typedef ON_OFF){

        pointerToTriac = this;              // assign current instance to pointer (IMPORTANT!!!)

        //Interruptions du Triac et Timer interne
        attachInterrupt(digitalPinToInterrupt(zc_pin), triacInterruptHandler, RISING);

        //Hardware timer 100uS
//        timer = timerBegin(0, 80, true);  //Clock Divider, 1 micro second Tick
//        timerAttachInterrupt(timer, &triacOnTimerHandler, true);
//        timerAlarmWrite(timer, 100, true);  //Interrupt every 100 Ticks or 100 microsecond
//        timerAlarmEnable(timer);

        
        timer = timerBegin(1000000);            // Set timer frequency to 1Mhz
        timerAttachInterrupt(timer, &triacOnTimerHandler);      // Attach onTimer function to our timer.
        timerAlarm(timer, 100, true, 0);        // Set alarm to call onTimer function every second (value in microseconds).
                                                // Repeat the alarm (third parameter) with unlimited count = 0 (fourth parameter).
    }

    // Interruption du Triac Signal Zc, toutes les 10ms
    
//    void IRAM_ATTR Triac::currentNull() {
    void ARDUINO_ISR_ATTR Triac::currentNull() {
            if (dimState == ON){ 
//            if ((millis() - lastZeroCross) > 2) {  // to avoid glitch detection during 2ms
                digitalWrite(dimmer_pin, LOW);  //Activate Triac (Stop current)
                zeroCross = true;
                pulseCompteur = 0;
                lastZeroCross = millis();
//            }
        } else if(zeroCross) {
            zeroCross = false;
            pulseCompteur = 0;
        }
    } 

    // Interruption Timer interne toutes les 100 micro secondes
//    void IRAM_ATTR Triac::onTimer() {  //Interruption every 100 micro second
    void ARDUINO_ISR_ATTR Triac::onTimer() {  //Interruption every 100 micro second
            if(zeroCross) {
            pulseCompteur += 1;
            if (pulseCompteur >= dimPulseBegin) {
                digitalWrite(dimmer_pin, HIGH);      //Stop Découpe Triac
                zeroCross = false;
                pulseCompteur = 0;
            }
        }
    }

    void Triac::setPower(int power){	
        if (power >= 99) {
            power = 99;
        }
        dimPower = power;
        dimPulseBegin = 100 - power;
        dimPulseEnd = power;
        delay(1);
    }

    int Triac::getPower(void){
        if (dimState == ON)
            return dimPower;
        else return 0;
    }

    void Triac::setState(ON_OFF_typedef ON_OFF){
        dimState = ON_OFF;
    }

    bool Triac::getState(void){
        bool ret;
        (dimState == ON) ? ret = true : ret = false;
        return ret;
    }


