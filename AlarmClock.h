//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef __ALARMCLOCK_H__
#define __ALARMCLOCK_H__

#include "Debug.h"
#include "Alarm.h"


namespace as {

#ifndef TICKS_PER_SECOND
  // default 100 ticks per second
  #define TICKS_PER_SECOND 100UL
#endif

#define seconds2ticks(tm) ( tm * TICKS_PER_SECOND )
#define ticks2seconds(tm) ( tm / TICKS_PER_SECOND )

#define decis2ticks(tm) ( tm * TICKS_PER_SECOND / 10 )
#define ticks2decis(tm) ( tm * 10UL / TICKS_PER_SECOND )

#define centis2ticks(tm)  ( tm * TICKS_PER_SECOND / 100 )
#define ticks2centis(tm)  ( tm * 100UL / TICKS_PER_SECOND )

#define millis2ticks(tm) ( tm * TICKS_PER_SECOND / 1000 )
#define ticks2millis(tm) ( tm * 1000UL / TICKS_PER_SECOND )

class AlarmClock: protected Link {

  Link ready;

public:

  void cancel(Alarm& item);

  AlarmClock& operator --();

  bool isready () const {
    return ready.select() != 0;
  }

  bool runready() {
    bool worked = false;
    Alarm* a;
    while ((a = (Alarm*) ready.unlink()) != 0) {
      a->trigger(*this);
      worked = true;
    }
    return worked;
  }

  void add(Alarm& item);

  uint32_t get(const Alarm& item) const;

  uint32_t next () const {
    Alarm* n = (Alarm*)select();
    return n != 0 ? n->tick : 0;
  }

  Alarm* first () const {
    return (Alarm*)select();
  }

  // correct the alarms after sleep
  void correct (uint32_t ticks) {
    ticks--;
    Alarm* n = first();
    if( n != 0 ) {
      uint32_t nextticks = n->tick-1;
      n->tick -= nextticks < ticks ? nextticks : ticks;
    }
    --(*this);
  }

};


extern void callback(void);

class SysClock : public AlarmClock {
public:
  void init() {
  #if ARDUINO_ARCH_AVR or ARDUINO_ARCH_ATMEGA32
  #define TIMER1_RESOLUTION 65536UL  // Timer1 is 16 bit
    // use Time1 on AVR
    TCCR1B = _BV(WGM13);        // set mode as phase and frequency correct pwm, stop the timer
    TCCR1A = 0;                 // clear control register A
    const unsigned long cycles = (F_CPU / 2000000) * (1000000 / TICKS_PER_SECOND);
    unsigned short pwmPeriod;
    unsigned char clockSelectBits;

    if (cycles < TIMER1_RESOLUTION) {
      clockSelectBits = _BV(CS10);
      pwmPeriod = (unsigned short)cycles;
    }
    else if (cycles < TIMER1_RESOLUTION * 8) {
      clockSelectBits = _BV(CS11);
      pwmPeriod = cycles / 8;
    }
    else if (cycles < TIMER1_RESOLUTION * 64) {
      clockSelectBits = _BV(CS11) | _BV(CS10);
      pwmPeriod = cycles / 64;
    }
    else if (cycles < TIMER1_RESOLUTION * 256) {
      clockSelectBits = _BV(CS12);
      pwmPeriod = cycles / 256;
    }
    else if (cycles < TIMER1_RESOLUTION * 1024) {
      clockSelectBits = _BV(CS12) | _BV(CS10);
      pwmPeriod = cycles / 1024;
    }
    else {
      clockSelectBits = _BV(CS12) | _BV(CS10);
      pwmPeriod = TIMER1_RESOLUTION - 1;
    }
    TCNT1 = 0;
    ICR1 = pwmPeriod;
    TCCR1B = _BV(WGM13) | clockSelectBits;
  #endif
  #ifdef ARDUINO_ARCH_STM32F1
    // Setup Timer2 on ARM
    Timer2.setMode(TIMER_CH2,TIMER_OUTPUT_COMPARE);
    Timer2.setPeriod(1000000 / TICKS_PER_SECOND); // in microseconds
    Timer2.setCompare(TIMER_CH2, 1); // overflow might be small
  #endif
    enable();
  }

  void disable () {
  #ifdef ARDUINO_ARCH_AVR
    TIMSK1 &= ~_BV(TOIE1);
  #endif
  #ifdef ARDUINO_ARCH_ATMEGA32
    TIMSK &= ~_BV(TOIE1);
  #endif
  #ifdef ARDUINO_ARCH_STM32F1
    Timer2.detachInterrupt(TIMER_CH2);
  #endif
  }

  void enable () {
  #ifdef ARDUINO_ARCH_AVR
    TIMSK1 |= _BV(TOIE1);
  #endif
  #ifdef ARDUINO_ARCH_ATMEGA32
     TIMSK |= _BV(TOIE1);
  #endif
  #ifdef ARDUINO_ARCH_STM32F1
    Timer2.attachInterrupt(TIMER_CH2,callback);
  #endif
  }

};

extern SysClock sysclock;



class RTC : public AlarmClock {
  uint8_t ovrfl;
public:

  RTC () : ovrfl(0) {}

  void init () {
#if ARDUINO_ARCH_AVR
    TIMSK2  = 0; //Disable timer2 interrupts
    ASSR  = (1<<AS2); //Enable asynchronous mode
    TCNT2 = 0; //set initial counter value
    TCCR2A = 0; // mode normal
    TCCR2B = (1<<CS22)|(1<<CS20); //set prescaller 128
    while (ASSR & ((1<<TCN2UB)|(1<<TCR2BUB))); //wait for registers update
    TIFR2  = (1<<TOV2); //clear interrupt flags
    TIMSK2  = (1<<TOIE2); //enable TOV2 interrupt
#elif ARDUINO_ARCH_ATMEGA32
    TIMSK &= ~(1<<TOIE2); //Disable timer2 interrupts
    ASSR  |= (1<<AS2); //Enable asynchronous mode
    TCNT2 = 0; //set initial counter value
    TCCR2 = (1<<CS22)|(1<<CS20); // mode normal & set prescaller 128
    while (ASSR & (1<<TCN2UB)); //wait for registers update
    TIFR |= (1<<TOV2); //clear interrupt flags
    TIMSK |= (1<<TOIE2); //enable TOV2 interrupt
#else
  #warning "RTC not supported"
#endif
  }

  uint16_t getCounter (bool resetovrflow) {
#if ARDUINO_ARCH_AVR or ARDUINO_ARCH_ATMEGA32
    if( resetovrflow == true ) {
      ovrfl = 0;
    }
    return (256 * ovrfl) + TCNT2;
#else
    return 0;
#endif
  }

  void overflow () {
    ovrfl++;
  }
};

extern RTC rtc;


extern Link pwm;

class SoftPWM : public Link {
private:
  uint32_t duty, value;
  uint8_t pin;

#define STEPS 100
#define FREQUENCE 2048
#define R ((STEPS * log10(2))/(log10(FREQUENCE)))

public:
  SoftPWM () : Link(0), duty(0), value(0), pin(0) {}

  void init (uint8_t p) {
    pin = p;
    pinMode(pin,OUTPUT);
    digitalWrite(pin,LOW);
    pwm.append(*this);
  }

  void count () {
    ++value;
    if( value == FREQUENCE ) {
      digitalWrite(pin,duty > 0 ? HIGH : LOW);
      value=0;
    }
    else if( value == duty ) {
      digitalWrite(pin,LOW);
    }
  }

  void set (uint8_t level) {
    duty = level > 0 ? pow(2,(level/R)) : 0;
  }
};


}

#endif
