//HOW IT SOUNDS
const float glissando = 0;  // how long it takes to track to a note; smaller numbers track faster or set to 0 for instant
const float vibrato_depth = 0;  // how much vibrato on each note; smaller numbers are less vibrato
const float vibrato_rate = 0.08;  // how quickly the vibrato vibratoes; smaller numbers are slower

//INTERNAL STUFF
int last_note = 0;
int note = 0;
const int num_octaves = 2;
const int num_notes = 12 * num_octaves + 1;
int notes[num_notes];
int notes_in_key = 0;  // how many notes fit in the range of the sensor
int key_offset = 0;
int key_type = 0;
int timer;  // used for keeping track of where we are in the vibrato
int sensor_input;
double last_xd;
const int note_lpf_buffer_size = 128;
int note_lpf_buffer[note_lpf_buffer_size];
const int keytype_lpf_buffer_size = 64;
int keytype_lpf_buffer[keytype_lpf_buffer_size];
const int keyoffset_lpf_buffer_size = 64;
int keyoffset_lpf_buffer[keyoffset_lpf_buffer_size];

int get_lpf_output(int *lpf_buffer, int lpf_buffer_size, int input)
{
  long sum = 0;
  
  for (int i = 0; i < lpf_buffer_size - 1; ++i)
  {
    lpf_buffer[i] = lpf_buffer[i + 1];
    sum += lpf_buffer[i];
  }

  lpf_buffer[lpf_buffer_size - 1] = input;
  sum += input;
  
  return sum / lpf_buffer_size;
}

void clear_lpf(int *lpf_buffer, int lpf_buffer_size)
{
  for (int i = 0; i < lpf_buffer_size; ++i)
    lpf_buffer[i] = 0;
}

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
  const double xcutoff = 14.0;  // value above which no notes will play
  const double xmin = 2.0;  // value below which the lowest note will play by default
  const double xmax = 9.0;  // value above which the highest note will play by default

  double xd = 1023.0 / x;

  //Serial.print(xd);
  //Serial.print(" - ");

  if (xd > xcutoff)
    xd = -1;  // return a negative value if the sensor value is below the cutoff
  else if (xd > xmax)
    xd = xmax;
  else if (xd < xmin)
    xd = xmin;

  //Serial.println(xd);

  if (xd < 0 || last_xd < 0)
  {
    //Serial.println("No output");
    last_xd = xd;
    return -1;
  }

  //Serial.println("Output!");
  //Serial.println(xd);
  last_xd = xd;

  double xnorm = 1.0 - (xd - xmin) / (xmax - xmin);
  return xnorm * 1024.0;
}

//resets the array of notees to choose from based on key type (maj, min, chr) and offset (in semitones) from A
void set_key(int type, int offset)
{
  int major[7] = {2, 2, 1, 2, 2, 2, 1};
  int minor[7] = {2, 1, 2, 2, 1, 2, 2};
  int chromatic[7] = {1, 1, 1, 1, 1, 1, 1};
  int *key_steps;

  if (type == 0)
  {
    key_steps = major;
    notes_in_key = 7 * num_octaves;
  }
  else if (type == 1)
  {
    key_steps = minor;
    notes_in_key = 7 * num_octaves;
  }
  else
  {
    key_steps = chromatic;
    notes_in_key = 12 * num_octaves;
  }

  note = 50 + offset;  //start 50 notes up from the minimum MIDI note
  
  for (int i = 0; i < num_notes; ++i)
  {
    note += key_steps[i % 7];
    notes[i] = note;
    //Serial.print(note);
    //Serial.print(", ");
  }

  key_type = type;
  key_offset = offset;
}

//converts the input of a pin (0-1024) to a defined range (e.g. 0-7)
int normalize(int x, int range)
{
  return int(range * x / 1024.0);
}

// plays a MIDI note. Doesn't check to see that cmd is greater than 127, or that
// data values are less than 127:
void send_midi(int cmd, int note, int velocity)
{
  /*Serial.print("Sending command: ");
  Serial.print(cmd);
  Serial.print(" ");
  Serial.print(note);
  Serial.print(" ");
  Serial.println(velocity);*/

  Serial.write(cmd);
  Serial.write(note);
  Serial.write(velocity);
}

//turns off all notes on all channels
void midi_kill()
{
  send_midi(176, 123, 0);
}

//needs to run at the start
void setup() {
  for (int i = 0; i < note_lpf_buffer_size; ++i)
    note_lpf_buffer[i] = 0;
    
  //Serial.begin(4800);
  Serial.begin(31250);
  set_key(0, 0);
  timer = 0;

  Serial.write(192);  // status byte for channel 0
  Serial.write(0);  // set instrument to 0
}

//the program loop
void loop() {  
  int input_key_type = normalize(
    get_lpf_output(keytype_lpf_buffer, keytype_lpf_buffer_size, pot_to_lin(analogRead(A2))), 
    3
  );
  
  int input_key_offset = normalize(
    get_lpf_output(keyoffset_lpf_buffer, keyoffset_lpf_buffer_size, pot_to_lin(analogRead(A1))), 
    12
  );

  if (input_key_type != key_type || input_key_offset != key_offset)
  {      
    set_key(input_key_type, input_key_offset);  // number of notes to shift up through notes array
    /*Serial.print("New key: ");
    Serial.print(key_type);
    Serial.print(", ");
    Serial.print(key_offset);
    Serial.println();*/
  }

  sensor_input = ir_to_lin(analogRead(A0));

  if (sensor_input == -1)  // if sensor value is below the cutoff
  {      
    midi_kill();
    clear_lpf(note_lpf_buffer, note_lpf_buffer_size);
    return;  // kill all midi notes, clear lpf buffer, do not pass go, do not collect $200
  }

  int index = notes_in_key * get_lpf_output(note_lpf_buffer, note_lpf_buffer_size, sensor_input) / 1024.0 + 0.5;
  note = notes[index];

  if (note != last_note)
  {
    midi_kill();
    send_midi(144, note, 127);
    //Serial.println(note);
  }

  last_note = note;
}
