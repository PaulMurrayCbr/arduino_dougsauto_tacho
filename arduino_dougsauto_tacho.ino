class MyStepper {
  public:
    const byte Apin, _Apin, Bpin, _Bpin;
    unsigned long lastMoveUs = 0;
    int location = 0;
    int zero = 0;
    int target = 0;
    const unsigned long maxSpeedUs;


    MyStepper( const byte ApinAttach, const byte  _ApinAttach,  const byte BpinAttach,  const byte _BpinAttach, const unsigned long maxSpeedUs) :
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

    void moveTo(int _target) {
      _target += zero;

      if (_target == target) return;

      if (location == target) {
        lastMoveUs = ((unsigned int) micros()) - maxSpeedUs;
      }

      target = _target;

    }

    void zeroHere() {
      zero = target;
    }

    inline boolean isMoving() {
      return location != target;
    }

};


/**
   I have a 48-pole stepper, and I want the dial
   to cover 3/4rds of the circle, so I am setting the steps to 48*3/4 = 36
*/

const int STEPS = 36;

// my stepper is pretty slow, so I will use 10000us as the maximum step speed
MyStepper tacho(3, 5, 6, 4, 10000);

void setup() {
  Serial.begin(57600);
  while (!Serial);

  tacho.setup();

  // to zero the tacho, we move forward STEPS * 1.25 the stop and then back by STEPS exactly;
  tacho.moveTo(STEPS * 5 / 4);
  while (tacho.isMoving()) tacho.loop();
  delay(250);
  tacho.zeroHere();

  tacho.moveTo(-STEPS);
  while (tacho.isMoving()) tacho.loop();
  delay(250);
  tacho.zeroHere();

  // and I move the needle between 0 and the limit just so I can see it for myself.

  for (int i = 0; i < 3; i++) {
    tacho.moveTo(STEPS);
    while (tacho.isMoving()) tacho.loop();
  delay(250);
    tacho.moveTo(0);
    while (tacho.isMoving()) tacho.loop();
  delay(250);
  }

}

void loop() {
  tacho.loop();

  dorandom();
}


void dorandom() {
  static unsigned long tt;
  if (millis() - tt < 1000) return;
  tt = millis();
  tacho.moveTo(random() % STEPS);
}



