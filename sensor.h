#define POT 0
#define IR 1

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

	if (xd > xcutoff)
		xd = -1;  // return a negative value if the sensor value is below the cutoff
	else if (xd > xmax)
		xd = xmax;
	else if (xd < xmin)
		xd = xmin;

	if (xd < 0)
	{
		return -1;
	}

	double xnorm = 1.0 - (xd - xmin) / (xmax - xmin);
	return xnorm * 1024.0;
}

class sensor
{
	private:
	int _pin;
	int _sensor_type;
	int _buffer_size;
	int *_buffer;
	int _num_states;
	int _last_state;
	int _has_changed;
	
	public:
	sensor(int pin, int sensor_type, int buffer_size, int num_states)
	{
		_pin = pin;
		_sensor_type = sensor_type;
		_buffer_size = buffer_size;
		_buffer = new int[buffer_size];
		_num_states = num_states;
		_last_state = 0;
		_has_changed = false;
	}
	
	int read()  // get the buffered state of the sensor normalized to the number of possible states
	{
		// read the sensor input and add it to the lpf buffer
		int input;
		
		for (int i = 0; i < _buffer_size - 1; ++i)
			_buffer[i] = _buffer[i + 1];
		
		if (_sensor_type == POT)
			input = pot_to_lin(analogRead(_pin));
		else if (_sensor_type == IR)
			input = ir_to_lin(analogRead(_pin));
		
		if (input == -1)  // sensor value is out of range (e.g. mute command), empty buffer and reset
		{
			clear_buffer();
			return -1;
		}
		
		_buffer[_buffer_size - 1] = input;
		
		// get the sensor's buffered value		
		long x = 0;
		
		for (int i = 0; i < _buffer_size; ++i)
			x += _buffer[i];
		
		x /= _buffer_size;  // value is now the buffered sensor value (0-1024)
		
		// normalize by the number of possible states
		x = int(x / 1024.0 * _num_states);  // x is now the sensor's state (0 to _num_states)
		
		if (x != _last_state)
			_has_changed = true;
		else
			_has_changed = false;
		
		_last_state = x;		
		return x;
	}
	
	bool has_changed()
	{
		return _has_changed;
	}
	
	void set_num_states(int n)  // sets the number of possible values the sensor can have
	{
		_num_states = n;
	}
	
	void clear_buffer()  // reset the sensor's buffer
	{
		for (int i = 0; i < _buffer_size; ++i)
			_buffer[i] = 0;
	}
};
		
		
	