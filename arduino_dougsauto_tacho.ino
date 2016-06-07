class MyStepper {
  public:
    const byte Apin, _Apin, Bpin, _Bpin;
    unsigned long lastMoveUs = 0;
    int location = 0;
    int zero = 0;
    int target = 0;
    const unsigned long maxSpeedUs;


    MyStepper(
      const byte ApinAttach, const byte  _ApinAttach,
      const byte BpinAttach,  const byte _BpinAttach,
      const unsigned long maxSpeedUs) :
      Apin(ApinAttach), _Apin(_ApinAttach), Bpin(BpinAttach), _Bpin(_BpinAttach), maxSpeedUs(maxSpeedUs)
    {}

    void setup() {
      pinMode(Apin, OUTPUT);
      pinMode(_Apin, OUTPUT);
      pinMode(Bpin, OUTPUT);
      pinMode(_Bpin, OUTPUT);
      lastMoveUs = micros();
    }

    void loop() {
      if (location == target) {
        return;
      }

      unsigned long nowUs =  micros();

      if (nowUs - lastMoveUs < maxSpeedUs) return;

      lastMoveUs = nowUs;

      location += target > location ? 1 : -1;

      // I turn on the HIGH before the low so that the
      // stepper is always being held.

      switch (location & 3) {
        case 0:
          digitalWrite(Apin,  HIGH);

          digitalWrite(Bpin,  LOW);
          digitalWrite(_Apin, LOW);
          digitalWrite(_Bpin, LOW);
          break;
        case 1:
          digitalWrite(Bpin,  HIGH);

          digitalWrite(Apin,  LOW);
          digitalWrite(_Apin, LOW);
          digitalWrite(_Bpin, LOW);
          break;
        case 2:
          digitalWrite(_Apin, HIGH);

          digitalWrite(Apin,  LOW);
          digitalWrite(Bpin,  LOW);
          digitalWrite(_Bpin, LOW);
          break;
        case 3:
          digitalWrite(_Bpin, HIGH);

          digitalWrite(Apin,  LOW);
          digitalWrite(Bpin,  LOW);
          digitalWrite(_Apin, LOW);
          break;
      }
    }

    inline int getTarget() {
      return target - zero;
    }

    void moveTo(int _target) {
      target = _target + zero;
    }

    void zeroHere() {
      zero = target;
    }

    inline boolean isMoving() {
      return location != target;
    }

};


////////////////////////////////////////////////////////////////////////////////

/*
    OK. This section includes a number of constants that will help you smooth
    the motion of the needle. As alsways, here's a trade off: the smoother you make the needle, \
    the less responsive it will be
*/


/*  this constant governs how fast the needle moves. For my stepper, it's pretty slow. Your
    instrument stepper should be much faster and use a much smaler value. If you make it too small,
    the stepper will lose track of where zero is - it's registration will slip.
*/

const unsigned long NEEDLE_MAX_RATE_us = 6000;

/**
   This constant coverns the low pass filter on the sampling. A value of 1 means no filtering.
   Higer values than this mean that the time between pulses gets averaged out, as a cost of the
   value taking time to reach the real value
*/

const int SAMPLE_LOW_PASS_FILTER = 4;

/**
 * 8000hz means a cycle lengh of 125us. This constant is a debouncer - the sketch will ignore 
 * pulses that are closer than this to the previously detected pulse. 
 */

const unsigned int SAMPLE_BOUNCE_LIMIT_us = 50; 

/**
   This value means that the sketch will not attempt to move the needle unless it has moved
   at least this far into the range of the next needle position segment.
*/

const double NEEDLE_HYSTERESIS = .5;

////////////////////////////////////////////////////////////////////////////////

/**
 * This section inclues various bounds and settings that together determine 
 * how the needle travels.
 */

/**
   I have a 48-pole stepper, and I want the dial
   to cover 3/4rds of the circle, so I am setting the steps to 48*3/4 = 36
*/

const int STEPS = 48 * 3 / 4;

// this is the frequency in Hz at which the steepper should be set to STEPS
// note that 
const double MAX_FREQ = 2050;

MyStepper tacho(3, 5, 6, 4, NEEDLE_MAX_RATE_us);

// this is the gear to count the width of the pulses

long pulseLengthUs;
long lastPulseUs;


void pulseISR() {
  unsigned long pulseUs = micros();
  unsigned long widthUs = pulseUs - lastPulseUs;

  // ignore glitches, potentially due to non-digital input on the pin
  if(widthUs < SAMPLE_BOUNCE_LIMIT_us) return;
  
  // this does a rolling average of the pulse length.
  pulseLengthUs = (pulseLengthUs * (SAMPLE_LOW_PASS_FILTER - 1) + (pulseUs - lastPulseUs)) / SAMPLE_LOW_PASS_FILTER;
  lastPulseUs = pulseUs;
}

void setup() {
  tacho.setup();

  // to zero the tacho, we move forward STEPS * 1.25 the stop and then back by STEPS exactly
  // this will zero it against the end stop at the limit of travel.
  
  tacho.moveTo(STEPS * 5 / 4);
  while (tacho.isMoving()) tacho.loop();
  tacho.zeroHere();

  tacho.moveTo(-STEPS);
  while (tacho.isMoving()) tacho.loop();
  tacho.zeroHere();

  // and I move the needle between 0 and the limit just so I can see it move.
  tacho.moveTo(STEPS);
  while (tacho.isMoving()) tacho.loop();
  tacho.moveTo(0);
  while (tacho.isMoving()) tacho.loop();

  // ok: hook up the tacho sensor.
  attachInterrupt(digitalPinToInterrupt(2), pulseISR, RISING);
}

void loop() {
  tacho.loop();

  // make this as fast as possible to reduce glitching
  noInterrupts();
  unsigned long pulseCpy = pulseLengthUs;
  interrupts();

  double newTarget;

  if (pulseLengthUs == 0)
    newTarget = 0;
  else {
    newTarget = 1000000.0 / (double)pulseCpy / MAX_FREQ * STEPS;
  }

  if (newTarget < 0) newTarget = 0;
  else if (newTarget > STEPS) newTarget = STEPS;

  // hysteresis
  if (newTarget < tacho.getTarget() - NEEDLE_HYSTERESIS || newTarget > tacho.getTarget() + 1 + NEEDLE_HYSTERESIS) {
    tacho.moveTo(newTarget);
  }
}


