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

  public:

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
      stepper.moveTo(STEPS * 5 / 4);
      while (stepper.isMoving()) stepper.loop();
      stepper.zeroHere();

      stepper.moveTo(-STEPS);
      while (stepper.isMoving()) stepper.loop();
      stepper.zeroHere();


      // and I move the needle between 0 and the limit just so I can see it move.
      //      stepper.moveTo(STEPS);
      //      while (stepper.isMoving()) stepper.loop();
      //      stepper.moveTo(0);
      //      while (stepper.isMoving()) stepper.loop();
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
      float avgPulseWidthCpyUs = avgPulseWidthUs;
      interrupts();
      
      double newTarget;

      if (avgPulseWidthCpyUs == 0 || avgPulseWidthCpyUs >= ZERO_SPEED_us)
        newTarget = 0;
      else {
        newTarget = 1000000.0 / avgPulseWidthCpyUs / MAX_FREQ * STEPS;
      }

      if (newTarget < 0) newTarget = 0;
      else if (newTarget > STEPS) newTarget = STEPS;

      // hysteresis
      if (newTarget < stepper.getTarget() - NEEDLE_HYSTERESIS || newTarget > stepper.getTarget() + 1 + NEEDLE_HYSTERESIS) {
        stepper.moveTo(newTarget);
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

void setup() {
  tacho.setup();
  tachoSampler.setup(); // the stepper must be set up before the sampler that uses it
  pinMode(2, INPUT);
  attachInterrupt(digitalPinToInterrupt(2), pin2ISR, RISING);

  speedo.setup();
  speedoSampler.setup(); // the stepper must be set up before the sampler that uses it
  pinMode(3, INPUT);
  attachInterrupt(digitalPinToInterrupt(3), pin3ISR, RISING);
}

void loop() {
  tacho.loop();
  tachoSampler.loop();
  speedo.loop();
  speedoSampler.loop();
}


