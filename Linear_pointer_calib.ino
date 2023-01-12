//* opto inputs
#define opto_1 2                  // linear lightsensor responsible for counting rotations (2 signals / revolution)
#define opto_2 3                  // linear lightsensor responsible for determining rotation direction
#define opto_cal 8                // linear lightsensor used for calibration

//* manual inputs
#define button 7                  // button used to switch between modes
#define potentioMeter A0          // pin to pull potentiometer values from (analog)

//* outputs
#define up 12                     // set to 1 to move up (digital)
#define down 4                    // set to 1 to move down (digital)
#define motor 5                   // pin to send motorPWM to (analog)
#define led A1                    // pin to write status led value to (analog)

//* variables
volatile long revCurrent = 0;     // stores the amount of half-rotations the pointer is away from origin, volatile is required for variables that can be changed by an interrupt
long revOld;                      // stores the previous amount of half-rotations (where the pointer was before it started moving to a new value)
long revDesired;                  // stores the value of half-rotations the pointer has to be away from origin to match potentiometer, or cli input
long revDelta;                    // stores the difference in half-rotations between the current position and the requested position

int revMin = 0;                  // minimun value in half-rotations the pointer is allowed to move to (editable)
int revMax = 380;                // maximum value in half-rotations the pointer is allowed to move to (editable)

int cmMin = 0;                   // minimum value in cm the pointer is allowed to move to (editable)
int cmMax = 15;                  // maximun value in cm the pointer is allowed to move to (editable)

unsigned char inputChars;        //stores the characters inputted via serial monitor

int mode;                        // stores a number ranged 0-2 which defines what mode the program is in (0=potentiometer, 1=cli, 2=calibrate)

unsigned long millisOld = 0;     // stores timestamp of previous blink (in milliseconds )
int ledstate = 0;                // stores pwm value to write to the status led

byte tens, units, number, numbers[2] = {0, 0}; // variable for storing data regarding the parsing of data inputted through the serial monitor

bool newNumber;                  // stores wether or not the latest value is the same as the previous value

int calibOffset = -7;             // offset to finetune calibration (editable)

//* setup
void setup ()
{
  //* serial
  Serial.begin(9600);

  //* mode
  mode = 0; // start in potentiometer mode

  //* interrupt
  attachInterrupt(digitalPinToInterrupt(opto_1), interrupt_cycle, RISING);  //attach an interrupt to opto_1, executing interrupt_cycle, whenever a rise in signal occurs
  
  //* pinmodes
  pinMode(motor, OUTPUT);
  pinMode(up, OUTPUT);
  pinMode(down, OUTPUT);
  pinMode(opto_1, INPUT);
  pinMode(opto_2, INPUT);
  pinMode(button, INPUT);
  
  //* setting all motor outputs to 0 
  digitalWrite(up, 0);
  digitalWrite(down, 0);
  analogWrite(motor, 0);
}

//* loop
void loop()
{
  if(digitalRead(button)) // if the button is pressed
  {
    update_mode();
  }

  switch(mode)
  {
    case 0:
      calibrate();
    break;

    case 1: // if mode 0 
      blink_led(120); // blinks led at frequency of 250ms
      track_potentiometer();
    break;

    case 2:
      blink_led(250); // blinks led at frequency of 500ms
      track_cli();
    break;
  }
}

//* functions 
void interrupt_cycle()
{
  if (digitalRead(opto_2))  // if opto_2 is true, while opto_1 is triggered (happens in only 1 of 2 directions)
  {
    revCurrent--;  // decrement position -> pointer moved "down"
  }
  else
  {
    revCurrent++;  // increment position -> pointer moved "up"
  }
}

//* used to write a pwm value to the motor
void move(int pwm)
{
  analogWrite(motor, 0); 
  revOld = revCurrent; 
  while (abs(revCurrent - revOld) <= 2)
  {
    analogWrite(motor, pwm);
  }
  analogWrite(motor, 0);
}

//* defines in which direction the pointer should move and subsequently supplies a set pwm value to the motor
void define_direction_and_move()
{
  revDelta = revDesired - revCurrent;
  if (abs(revDelta) > 2)    // the difference between the current position and the requested position has to be greater than 2 half-rotations  
  {
    if (revDelta < 0)   // if the pointer needs to move to a lower value
    {
      digitalWrite(up, 0);
      digitalWrite(down, 1);  // set down to true
    }
    else  // if the pointer needs to move to a higher value
    {
      digitalWrite(up, 1);    // set up to true
      digitalWrite(down, 0);
    }

    if (abs(revDelta) >= 50 ) // if revDelta is greater than 50 half-rotations, the pointer should move at a high speed
    {
      move(255);
    }
    else
    {
      if (abs(revDelta) >= 15 ) // if revdelta is between 50 and 15 half-revolutions the pointer should move at a medium speed
      {
        move(225);
      }
      else // if revDelta is smaller than 15 half-revolutions the pointer should move at a low speed
      {
        move(200);  
      }
    }
  }
}

//* automatic calibration
void calibrate()
{
  while(!digitalRead(opto_cal)) //untill opto_cal gets triggered
  {
    digitalWrite(up, 1);
    digitalWrite(down, 0);
    analogWrite(motor, 255);
  }
  
  analogWrite(motor, 0);
  digitalWrite(up, 0);   

  revCurrent = (revMax + calibOffset);  // pointer is at maximum value (because that's where the calibration slot is)
  
  mode =1;

}

//* tracks the pointer to the numbers inputted through the serial monitor
void track_cli(){
  if (Serial.available())
  {
    inputChars = Serial.read(); 
    if (inputChars >= '0' && inputChars <= '9') 
    {
      newNumber = true; 
      numbers[0] = numbers[1];
      numbers[1] = inputChars - '0';
    }
    else 
    {
      if (newNumber)
      {
        newNumber = false; 
        number = numbers[0] * 10 + numbers[1];
        if (number <= 15) 
        {
          revDesired = map(number, cmMin, cmMax, revMin, revMax); // remaps number form 0<x<15 to a representable value in half-rotations, and sets it to revDesired 
        }
        numbers[1] = 0;
      }
    }
  }

  define_direction_and_move();
}

//* tracks the pointer to the potentiometer value
void track_potentiometer()
{
  int potValue = analogRead(potentioMeter);
  revDesired = map(potValue, 0, 1032, revMin, revMax);

  define_direction_and_move();
}

//* blinks the status led 
void blink_led(int frequency)
{
  unsigned long millisCurrent = millis();   // get current timestamp
  
  if(millisCurrent - millisOld >= frequency)   // if time since last blink is greater or equal to the blink frequency
  {
    millisOld = millisCurrent;    

    if(!ledstate) // if led was off 
    {
      ledstate = 255;   // turn led on     
    }
    else  // if led was on
    {
      ledstate = 0;   // turn led off
    }

    analogWrite(led, ledstate);   // write pwm value to led
  }
}

//* updates the mode using the button, by cycling  through all 3 modes; calibration / cli control / potentiometer control
void update_mode()
{
  if(mode == 1)  // if mode is 0 
  {
    mode= 2;
  }
  else if (mode == 2)  // if mode is 2
  {
    mode = 1;
  }
  while(digitalRead(button));   // wait for button to be released
}