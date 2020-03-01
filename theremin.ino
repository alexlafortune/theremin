#include "sensor.h"

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

sensor note_sensor = sensor(A0, IR, 128, 0);
sensor volume_sensor = sensor(A3, IR, 128, 127);
sensor keytype_sensor = sensor(A2, POT, 64, 3);
sensor transpose_sensor = sensor(A1, POT, 64, 12);
//sensor note_sensor, volume_sensor, keytype_sensor, transpose_sensor;

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
	}

	key_type = type;
	key_offset = offset;
}

// plays a MIDI note. Doesn't check to see that cmd is greater than 127, or that
// data values are less than 127:
void send_midi(int cmd, int note, int velocity)
{
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
void setup()
{
	//Serial.begin(4800);
	Serial.begin(31250);
	set_key(0, 0);
	timer = 0;

	Serial.write(192);  // status byte for channel 0
	Serial.write(0);  // set instrument to 0
	
	note_sensor.set_num_states(notes_in_key);
}

//the program loop
void loop()
{  
	int keytype = keytype_sensor.read();
	int transpose = transpose_sensor.read();

	if (keytype_sensor.has_changed() || transpose_sensor.has_changed())
	{      
		set_key(keytype, transpose);
		note_sensor.set_num_states(notes_in_key);
	}

	int index = note_sensor.read();

	if (index < 0)  // if sensor value is below the cutoff
	{      
		midi_kill();
		return;  // kill all midi notes, clear lpf buffer, do not pass go, do not collect $200
	}

	note = notes[index];
	
	int volume = volume_sensor.read();
	
	if (volume < 0)
		volume = 127;
	else
		volume = 127 - volume;  // hand close to the sensor means low volume

	if (note_sensor.has_changed())
	{
		midi_kill();
		send_midi(144, note, volume);
	}

	last_note = note;
}
