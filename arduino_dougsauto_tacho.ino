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

      switch (location & 7) {
        case 0:
          digitalWrite(Apin,  HIGH);
          digitalWrite(_Apin, LOW);
          digitalWrite(Bpin,  LOW);
          digitalWrite(_Bpin, LOW);
          break;
        case 1:
          digitalWrite(Apin,  HIGH);
          digitalWrite(_Apin, LOW);
          digitalWrite(Bpin,  HIGH);
          digitalWrite(_Bpin, LOW);
          break;
        case 2:
          digitalWrite(Apin,  LOW);
          digitalWrite(_Apin, LOW);
          digitalWrite(Bpin,  HIGH);
          digitalWrite(_Bpin, LOW);
          break;
        case 3:
          digitalWrite(Apin,  LOW);
          digitalWrite(_Apin, HIGH);
          digitalWrite(Bpin,  HIGH);
          digitalWrite(_Bpin, LOW);
          break;
        case 4:
          digitalWrite(Apin,  LOW);
          digitalWrite(_Apin, HIGH);
          digitalWrite(Bpin,  LOW);
          digitalWrite(_Bpin, LOW);
          break;
        case 5:
          digitalWrite(Apin,  LOW);
          digitalWrite(_Apin, HIGH);
          digitalWrite(Bpin,  LOW);
          digitalWrite(_Bpin, HIGH);
          break;
        case 6:
          digitalWrite(Apin,  LOW);
          digitalWrite(_Apin, LOW);
          digitalWrite(Bpin,  LOW);
          digitalWrite(_Bpin, HIGH);
          break;
        case 7:
          digitalWrite(Apin,  HIGH);
          digitalWrite(_Apin, LOW);
          digitalWrite(Bpin,  LOW);
          digitalWrite(_Bpin, HIGH);
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

class Sampler {
  public:
    // make this static because otherwise i have to malloc things, which is a pain
    static const int RING_BUFFER_SIZE = 50;
    const float LOWPASS_DAMPING;
    const float MAX_FREQ;
    const int STEPS;
    const float NEEDLE_HYSTERESIS;

    // a signal coming in at this rate will have a frequency
    // definitely below MIN_FREQ.
    const unsigned long ZERO_SPEED_us;

    MyStepper &stepper;

    volatile long mostRecentPulseUs;
    volatile long ringBufferUs[RING_BUFFER_SIZE];
    volatile int ringBufferPos = 0;
    volatile float avgPulseWidthUs;

    // this is a value that it's safe to inspect from outside an interrupt
    float avgPulseWidthCpyUs;
    float frequencyHz;

    Sampler(const float LOWPASS_DAMPING, const float MAX_FREQ,  const float MIN_FREQ, const int STEPS, const float NEEDLE_HYSTERESIS, MyStepper &stepper ) :
      LOWPASS_DAMPING(LOWPASS_DAMPING) ,
      MAX_FREQ(MAX_FREQ),
      STEPS(STEPS),
      NEEDLE_HYSTERESIS(NEEDLE_HYSTERESIS),
      stepper(stepper),
      ZERO_SPEED_us(1000000 / MIN_FREQ)
    {
    }

    void pulseISR() {
      if (++ringBufferPos >= RING_BUFFER_SIZE) {
        ringBufferPos = 0;
      }

      mostRecentPulseUs = micros();
      unsigned long pulseWidthUs = mostRecentPulseUs - ringBufferUs[ringBufferPos];
      ringBufferUs[ringBufferPos] = mostRecentPulseUs;

      // these floating point operations may cause problems on account of being slow and hapenning inside an ISR

      avgPulseWidthUs = LOWPASS_DAMPING * avgPulseWidthUs + (1 - LOWPASS_DAMPING) * (float)pulseWidthUs / RING_BUFFER_SIZE;
    }

    void setup() {
    }

    void loop() {
      // make this as fast as possible to reduce glitching
      noInterrupts();

      // if the mst recent pulse was a long time ago (as specified by ZERO_SPEED_us),
      // then we manually zero the speed
      if (micros() - mostRecentPulseUs > ZERO_SPEED_us && avgPulseWidthUs <= ZERO_SPEED_us) {
        avgPulseWidthUs = ZERO_SPEED_us * 2.0; // times two to give us plenty of slop
      }

      // this needs to be in a nointerrupts block because
      // flat read and writes are not necessarily atomic
      avgPulseWidthCpyUs = avgPulseWidthUs;
      interrupts();

      double newTarget;

      if (avgPulseWidthCpyUs == 0 || avgPulseWidthCpyUs >= ZERO_SPEED_us)
        frequencyHz = 0;
      else
        frequencyHz = 1000000.0 / avgPulseWidthCpyUs;

      newTarget = frequencyHz / MAX_FREQ * STEPS;
      if (newTarget < 0) newTarget = 0;
      else if (newTarget > STEPS) newTarget = STEPS;

      // hysteresis
      if (newTarget < stepper.getTarget() - NEEDLE_HYSTERESIS || newTarget > stepper.getTarget() + 1 + NEEDLE_HYSTERESIS) {
        stepper.moveTo(newTarget);
      }
    }

};

class TurnPinOnIfSamplerOverLimit {
  public:
    const float lowerBoundHz;
    const float upperBoundHz;
    Sampler &sampler;
    const byte pin;
    boolean state;

    TurnPinOnIfSamplerOverLimit( const float lowerBoundHz,
                                 const float upperBoundHz,
                                 Sampler &sampler,
                                 const byte pin) :
      lowerBoundHz(lowerBoundHz),
      upperBoundHz(upperBoundHz),
      sampler(sampler),
      pin(pin)
    {}

    void setup() {
      pinMode(pin, OUTPUT);
      state = digitalRead(pin) != LOW;
    }

    void loop() {
      if (state && sampler.frequencyHz < lowerBoundHz) {
        state = false;
        digitalWrite(pin, LOW);
      }
      else if (!state &&  sampler.frequencyHz >= upperBoundHz) {
        state = true;
        digitalWrite(pin, HIGH);
      }
    }

};

/*

   I need pin 13 to activate ONLY when the frequency is between 160 to 165 HZ so off
   anything below 160 and off at anything above165 only on at 160 to 165 HZ.
   but here is the difficult part. when the frequency is rising, when it passes
   through 160 to 165 HZ pin 13 should not activate. it should only activate when the
   frequency is decreasing as it passes from 165 to160HZ below 160HZ  it should be off.

*/

class TurnPinOnWhenFallingThroughWindow {
  public:
    const float lowerBoundHz;
    const float upperBoundHz;
    Sampler &sampler;
    const byte pin;
    enum State {
      ABOVE_WINDOW = 3, IN_WINDOW = 2, BELOW_WINDOW = 1
    } state;

    TurnPinOnWhenFallingThroughWindow( const float lowerBoundHz,
                                       const float upperBoundHz,
                                       Sampler &sampler,
                                       const byte pin) :
      lowerBoundHz(lowerBoundHz),
      upperBoundHz(upperBoundHz),
      sampler(sampler),
      pin(pin)
    {}

    void setup() {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
      state = BELOW_WINDOW;
    }

    void loop() {
      if (sampler.frequencyHz < lowerBoundHz) {
        if (state != BELOW_WINDOW) {
          digitalWrite(pin, LOW);
        }
        state = BELOW_WINDOW;
      }
      else if (sampler.frequencyHz > upperBoundHz) {
        if (state != ABOVE_WINDOW) {
          digitalWrite(pin, LOW);
        }
        state = ABOVE_WINDOW;
      }
      else  {
        switch (state) {
          case BELOW_WINDOW:
            digitalWrite(pin, LOW);
            break;
          case IN_WINDOW:
            // leave the pin as-is
            break;
          case ABOVE_WINDOW:
            digitalWrite(pin, HIGH);
            break;
        }
        state = IN_WINDOW;
      }
    }
};

MyStepper tacho(5, 7, 8, 6, 3000); // pins 4,5,6,4, max rate 6000us because I have flattened my batteries :(
MyStepper speedo(9, 11, 12, 10, 3000); // pins 4,5,6,4, max rate 6000us because I have flattened my batteries :(

Sampler tachoSampler(
  .75, // low-pass damping. 0 means none, 1 means no signal at all
  2050,   // max frequency
  5,   // min frequency
  48 * 2 * 3 / 4, // number of steps on the stepper
  .6, // hysteresis at .5, there is a microscopic chance of needle vibration, so set it to just over that
  tacho // the stepper motor to which this instance is bound
);

Sampler speedoSampler(
  .75, // low-pass damping. 0 means none, 1 means no signal at all
  2050,   // max frequency
  512,   // min frequency
  48 * 2 * 3 / 4, // number of steps on the stepper
  .6, // hysteresis at .5, there is a microscopic chance of needle vibration, so set it to just over that
  speedo // the stepper motor to which this instance is bound
);

void pin2ISR() {
  tachoSampler.pulseISR();
}

void pin3ISR() {
  speedoSampler.pulseISR();
}

// pin 4 goes on if speedo over 1000 and off when it drops below 950
TurnPinOnIfSamplerOverLimit speedoLimit(950, 1000, speedoSampler, 4);

// pin 13 goes on if tacho between 160 and 165 AND dropping
TurnPinOnWhenFallingThroughWindow tachoLimit(160, 165, tachoSampler, 13);

void setup() {
  tacho.setup();
  tachoSampler.setup(); // the stepper must be set up before the sampler that uses it
  pinMode(2, INPUT);
  attachInterrupt(digitalPinToInterrupt(2), pin2ISR, RISING);

  speedo.setup();
  speedoSampler.setup(); // the stepper must be set up before the sampler that uses it
  pinMode(3, INPUT);
  attachInterrupt(digitalPinToInterrupt(3), pin3ISR, RISING);

  speedoLimit.setup();
  tachoLimit.setup();

  tacho.moveTo(tachoSampler.STEPS * 5 / 4);
  speedo.moveTo(speedoSampler.STEPS * 5 / 4);
  while (tacho.isMoving() || speedo.isMoving()) {
    tacho.loop();
    speedo.loop();
  }
  tacho.zeroHere();
  speedo.zeroHere();
  tacho.moveTo(-tachoSampler.STEPS);
  speedo.moveTo(-speedoSampler.STEPS);
  while (tacho.isMoving() || speedo.isMoving()) {
    tacho.loop();
    speedo.loop();
  }
  tacho.zeroHere();
  speedo.zeroHere();
}

void loop() {
  tacho.loop();
  tachoSampler.loop();
  speedo.loop();
  speedoSampler.loop();

  speedoLimit.loop();
  tachoLimit.loop();

}


