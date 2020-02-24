#include "pitches.h"

//HOW IT SOUNDS
const float glissando = 20000;  // how long it takes to track to a note; smaller numbers track faster or set to 0 for instant 
const float vibrato_depth = 0.1;  // how much vibrato on each note; smaller numbers are less vibrato
const float vibrato_rate = 0.001;  // how quickly the vibrato vibratoes; smaller numbers are slower

//INTERNAL STUFF
int out_pin = 8;
float last_pitch = 0;
float pitch = 0;
float target_pitch = 0;
const int num_pitches = sizeof(pitches) / sizeof(float);
const int num_octaves = 2;
int key_index = 0;
int key_type = 0;
int last_key_index = -1;
int last_key_type = -1;
float key_pitches[num_pitches];
int range = 0;
int timer;  // used for keeping track of where we are in the vibrato
int sensor_input;

//converts a potentiometer signal into a more linear signal
int pot_to_lin(int x)
{
  const double x0 = 148;  //  the slope changes about halfway through pot's rotation 
  const double y0 = 512;  // pot value where the slope changes
  const double m = (1024 - y0) / (1024 - x0);
  const double b = y0 - m * x0;  // y-intercept of second sloped section

  if (x < x0)
    return y0 / x0 * x;
  else
    return m * x + b;
}

//converts the IR distance sensor signal into a more linear signal
//note: xmin and xmax can be adjusted to change how physically far the highest and lowest note are from each other 
int ir_to_lin(int x)
{
  const double xcutoff = 2.0;  // value below which no notes will play
  const double xmin = 3.0;  // value below which the lowest note will play by default
  const double xmax = 10.0;  // value above which the highest note will play by default

  double xd = 1023.0 / x;

  if (xd < xcutoff)
    return -1;  // return a negative value if the sensor value is below the cutoff
  else if (xd > xmax)
    xd = xmax;
  else if (xd < xmin)
    xd = xmin;

  double xnorm = 1.0 - (xd - xmin) / (xmax - xmin);  
  return xnorm * 1024.0;
}

//takes a float and returns the nearest index of the pitches array
int snap(float pitch)
{
  for (int i = 0; i < num_pitches; ++i)
    if (pitches[i] >= pitch)
      return i;
}

//resets the array of pitches to choose from based on key type (maj, min, chr) and offset (in semitones) from A
void set_key(int key_type, int offset)
{  
  int major[7] = {2, 2, 1, 2, 2, 2, 1};
  int minor[7] = {2, 1, 2, 2, 1, 2, 2};
  int chromatic[7] = {1, 1, 1, 1, 1, 1, 1};
  int *key_steps;
  
  if (key_type == 0)
  {
    key_steps = major;
    range = num_octaves * 7;
  }
  else if (key_type == 1)
  {
    key_steps = minor;
    range = num_octaves * 7;
  }
  else
  {
    key_steps = chromatic;
    range = num_octaves * 12;
  }

  for (int i = offset, j = 0; i < num_pitches; ++j)
  {
    key_pitches[j] = pitches[i];
    i += key_steps[j % 7];
  }
}

//converts the input of a pin (0-1024) to a defined range (e.g. 0-7)
int input(int pin, int range)
{
  return int(range * pot_to_lin(analogRead(pin)) / 1024.0);
}

//needs to run at the start
void setup() {
  pinMode(out_pin, OUTPUT);
  Serial.begin(4800);
  set_key(0, 0);
  timer = 0;
}

//the program loop itself
void loop() {
  key_index = input(A1, 12);
  key_type = input(A2, 3);
  
  if (last_key_index != key_index || last_key_type != key_type)
    set_key(key_type, key_index);  // semitones to shift up through pitches array

  sensor_input = ir_to_lin(analogRead(A0));

  if (sensor_input == -1)  // if sensor value is below the cutoff
    return;  // play no notes, do not pass go, do not collect $200
  
  int index = range * sensor_input / 1024.0 + 0.5;
  target_pitch = key_pitches[index];  
  // round to nearest index, not just round down, which gives access to notes 0 thru 11 plus the octave, 12

  if (glissando == 0 || last_pitch == 0)  // don't bother doing math if glissando is zero
  {
    pitch = target_pitch;
  }
  else
  {
    if (target_pitch > last_pitch)  // adjust the pitch based on how far we are from the last pitch
      pitch = last_pitch * (1 + abs(target_pitch - last_pitch) / glissando);
    else
      pitch = last_pitch / (1 + abs(target_pitch - last_pitch) / glissando);
  }

  if (vibrato_depth > 0)  // add a bit of a sine wave to do vibrato if enabled
  {
    pitch += pitch * vibrato_depth * sin(timer * vibrato_rate);
  }

  tone(out_pin, pitch, 50);  // maps sensor input
  last_pitch = pitch;
  ++timer; // increment vibrato timer
}
