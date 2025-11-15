
#include <Arduino.h>

#ifndef __TRIACVARS
#define __TRIACVARS				

    typedef enum{ 
        OFF = false,
        ON = true
    } ON_OFF_typedef;

#endif

    extern const bool debug;


class Triac {         
    private:
		
        int zc_pin;
        hw_timer_t *timer = NULL;

        uint16_t dimPower = 0;

        int dimmer_pin;
        volatile ON_OFF_typedef dimState = OFF;
        volatile uint16_t pulseCompteur = 0;
        volatile bool zeroCross = false;
        volatile unsigned long lastZeroCross = 0;
        uint16_t dimPulseBegin = 100;
        uint16_t dimPulseEnd = 0;

    public:   
		
        Triac(int user_dimmer_pin, int zc_dimmer_pin);
        ~Triac(void);

        void IRAM_ATTR currentNull();   // Interruption du Triac Signal Zc, toutes les 10ms
        void IRAM_ATTR onTimer();       //Interruption every 100 micro second

        void begin(ON_OFF_typedef ON_OFF);
        void setPower(int power);
		int  getPower(void);
		void setState(ON_OFF_typedef ON_OFF);
        bool getState(void);
};
