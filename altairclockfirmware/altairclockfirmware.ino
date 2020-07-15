//time set button
//+ and - buttons

//NOTE - max current draw per LED is 14mA, so that we do not go over the 200mA for the arduino.
//Ultrabright red 5mm LED with 160 Ohm resistor gives us that.

#include <avr/power.h>

//CONTROLS
#define PIN_BTN_SET  1
#define PIN_BTN_UP   2
#define PIN_BTN_DN 3

#define PIN_MASTERMODE_LIGHT 19

#define MASTERMODE_RUN 1000
#define MASTERMODE_SETHOUR 250
#define MASTERMODE_SETMINUTE 125

#define BUTTON_WAITING 2
#define BUTTON_ON 1
#define BUTTON_OFF 0

#define MINUTE_RUNTIME 59980

int mastermode = MASTERMODE_RUN;
int last_frame_duration;

//DISPLAY
byte PIN_LED_TOP[] = {10,16,14,15,18,19};
byte PIN_LED_BOTTOM[] = {9,8,7,6,5,4};
byte temparray_hour[6];
byte temparray_minute[6];
byte time_hour = 12;
byte time_minute = 30;

class timer
{
  private:  
  long value = 0;
  long target;
  byte flag = 0;

  public:
  timer(long l) {target=l;}
  void setTarget(long t) {target = t;}
  byte Flag() {return flag;}
  void reset() {value = target; flag = 0;}  
  void update(int d)
  {
    value -= (long)d;
    if (value < 0)
    {
      value = target;
      flag = 1-flag; 
    }   
  }
};

class button
{
  private:
  unsigned long int press_start = 0;
  unsigned long int press_end = 0;
  int threshold;
  byte state = 0;
  byte pin = 0;
  
  public:
  button(byte p, int t) {pin = p; threshold = t;}
  void reset() {state = BUTTON_OFF; }  
  byte State() {return state;}
    
  void update()
  {    
    if (digitalRead(pin) == LOW)
    {                         
         if (press_start == 0)
         {
          state = BUTTON_WAITING;
          press_start = millis();
         }
         press_end = 0;
    }
    else
    {
      if (press_end == 0)
      {
        press_end = millis();
      }      
    }

    if (press_start && press_end)
    {
      int d = int(press_end - press_start);
      if (d > threshold)
      {
        state = BUTTON_ON;
      }
      else
      {
        state = BUTTON_OFF;
      }
      press_start = 0;      
    }
  }
};

timer* timer_mastermode_flash = new timer(MASTERMODE_RUN);
timer* timer_clockupdate = new timer(MINUTE_RUNTIME);
timer* timer_returntorun = new timer(15000);

button* button_set = new button(PIN_BTN_SET, 500);
button* button_up = new button(PIN_BTN_UP, 1);
button* button_down = new button(PIN_BTN_DN, 1);

void update_timers(int d)
{
  timer_mastermode_flash->update(d);    
  timer_clockupdate->update(d);
  timer_returntorun->update(d);
}

void DecToBin(byte hour, byte minute)
{
  for(int i = 0; i < 6; i++)
  {
    temparray_hour[i] = bitRead(hour, i);
    temparray_minute[i] = bitRead(minute, i);
  }    
}

// does the flashing effect on the LEDs
void flash_top()
{
  for (int i = 0; i < 5; i++)
  {    
    if (temparray_hour[i] == 0)
    {
      digitalWrite(PIN_LED_TOP[i], LOW);
      continue;
    }
    
    if (random(10) > 3)
    {
      digitalWrite(PIN_LED_TOP[i], HIGH);
    }
    else
    {
      digitalWrite(PIN_LED_TOP[i], LOW);
    }    
  }  
}

void flash_bottom()
{
  for (int i = 0; i < 6; i++)
  {    
    if (temparray_minute[i] == 0)
    {
      digitalWrite(PIN_LED_BOTTOM[i], LOW);
      continue;
    }
    
    if (random(10) > 3)
    {
      digitalWrite(PIN_LED_BOTTOM[i], HIGH);
    }
    else
    {
      digitalWrite(PIN_LED_BOTTOM[i], LOW);
    }
  }
}

//renders the main display. Rapidly alternates between
//rendering top and bottom rows to avoid overloading
//current on GPIO pins
void RenderDisplay()
{ 
  DecToBin(time_hour, time_minute);  
  flash_top();
  flash_bottom();
  digitalWrite(PIN_MASTERMODE_LIGHT, timer_mastermode_flash->Flag());
}

void setup()
{
  //disable some devices to reduce power
  power_adc_disable();
  power_spi_disable();
  power_twi_disable();

  //setup buttons
  pinMode(PIN_BTN_SET, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DN, INPUT_PULLUP);

  //setup display
  for (int i = 0; i < 6; i++)
  {
    pinMode(PIN_LED_TOP[i], OUTPUT);    
    digitalWrite(PIN_LED_TOP[i], LOW);
    
    pinMode(PIN_LED_BOTTOM[i], OUTPUT);
    digitalWrite(PIN_LED_BOTTOM[i], LOW);    
  }

}

void handle_input()
{
  button_set->update();
  button_up->update();
  button_down->update();
      
  switch(mastermode)
  {
    case MASTERMODE_RUN:
      timer_returntorun->reset();
      if (button_set->State() == BUTTON_ON)
      {
        mastermode = MASTERMODE_SETHOUR;
        timer_mastermode_flash->setTarget(MASTERMODE_SETHOUR);
        button_set->reset();
      }
    break;
    case MASTERMODE_SETHOUR:
      if (timer_returntorun->Flag())
      {
        mastermode = MASTERMODE_RUN;  
        timer_mastermode_flash->setTarget(MASTERMODE_RUN);          
      }
      else
      {      
        if (button_set->State() == BUTTON_ON)
        {          
          mastermode = MASTERMODE_SETMINUTE;
          timer_mastermode_flash->setTarget(MASTERMODE_SETMINUTE);
          button_set->reset();          
        }
        if (button_up->State() == BUTTON_ON)
        {
          time_hour++;
          if (time_hour > 23)
          {
            time_hour = 0;
          }
          button_up->reset();
          timer_returntorun->reset();
        }
        if (button_down->State() == BUTTON_ON)
        {
          time_hour--;
          if (time_hour > 100)
          {
            time_hour = 23;
          }
          button_down->reset();
          timer_returntorun->reset();
        }
      }
    break;
    case MASTERMODE_SETMINUTE:
     if (timer_returntorun->Flag())
      {
        mastermode = MASTERMODE_RUN;    
        timer_mastermode_flash->setTarget(MASTERMODE_RUN);        
      }
      else
      { 
        if (button_set->State() == BUTTON_ON)
        {
          
          mastermode = MASTERMODE_RUN;
          timer_mastermode_flash->setTarget(MASTERMODE_RUN);
          button_set->reset();
        }
        if (button_up->State() == BUTTON_ON)
        {
          time_minute++;
          if (time_minute > 59)
          {
            time_minute = 0;
          }
          button_up->reset();
          timer_returntorun->reset();
        }
        if (button_down->State() == BUTTON_ON)
        {
          time_minute--;
          if (time_minute > 100)
          {
            time_minute = 59;
          }
          button_down->reset();
          timer_returntorun->reset();
        }
      }
    break;              
    }    
}

void loop_core()
{
  unsigned long int s = micros();  
  if (timer_clockupdate->Flag())
  {
    timer_clockupdate->reset();  
    time_minute++;
    if (time_minute > 59)
    {
      time_minute = 0;
      time_hour++;
      if (time_hour > 23)
      {
        time_hour = 0;
      }
    }
  }  
  handle_input();  
  RenderDisplay();
  unsigned long int e = micros();
}

void loop()
{ 
  unsigned long int s = millis();
  loop_core();  
  delay(20);
  unsigned long int e = millis();
  last_frame_duration = (int)(e-s);
  update_timers(last_frame_duration);
}
