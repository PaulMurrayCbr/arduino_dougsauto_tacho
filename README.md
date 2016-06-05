# arduino_dougsauto_tacho
Code to run a switec x25 from a squarewave input. 0 to 2050 HZ @8000 RPM . I belive the OP specified a 600-position stepper (an instrument stepper), but I only have a 24-step so I'll make it configurable.

At startup, the motor should swing all the way to the right so as to hit the stop and then wind back to zero.

I'll use https://github.com/clearwater/SwitecX25 . I belive it will work with my stepper, and since the library is meant to work with the instrument stepper, if my code drives it ok then it should drive the client's stepper no problem.


