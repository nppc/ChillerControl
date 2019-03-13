#define FIRMWAREVERSION "3.0.0"

// Default values
#define DEFAULT_HYSTERESIS 0.1  // 0.2 °C wide
#define DEFAULT_PID_KP 10        
#define DEFAULT_PID_KI 0.005
#define DEFAULT_PID_KD 0.2

#define PIDCOMPUTE_INTERVAL 5	// seconds, but not faster than 1 second (temperature sensor needs time)
#define I2C_INTERVAL 2			// seconds, Send/Request data from I2C Buck 

// other constants
#define RADIATOR_EXTREME_TEMP 45.0	// If radiator temperature is greater that this constant, then reduce power
#define MAXIMUM_PWM_HOT_WIRE 700 // Maximum PWM for hot wire. full range will give too much current, so limit it.
