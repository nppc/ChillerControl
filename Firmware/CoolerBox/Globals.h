#define FIRMWAREVERSION "3.2"

// Default values
#define DEFAULT_HYSTERESIS 0.05  // ±0.05 °C (0.10 °C total deadband span)
#define DEFAULT_PID_KP 10        
#define DEFAULT_PID_KI 0.005
#define DEFAULT_PID_KD 0.2

#define PIDCOMPUTE_INTERVAL 1	// seconds, continuous 12-bit DS18B20 conversion
#define I2C_INTERVAL 2			// seconds, Send/Request data from I2C Buck 

#define ENABLE_ISPINDEL_DYNAMIC_TEMP 0 // Disabled until an iSpindel input source is implemented.

// other constants
#define RADIATOR_EXTREME_TEMP 45.0	// If radiator temperature is greater that this constant, then reduce power
#define MAXIMUM_PWM_HOT_WIRE 700.0 // Maximum PWM for hot wire. full range will give too much current, so limit it.
