// Default values
#define DEFAULT_HYSTERESIS 0.05  // ±0.05 °C (0.10 °C total deadband span)
#define DEFAULT_PID_KP 60.0        
#define DEFAULT_PID_KI 0.080
#define DEFAULT_PID_KD 15.0

#define PIDCOMPUTE_INTERVAL 2	// seconds, continuous 12-bit DS18B20 conversion
#define I2C_INTERVAL 2			// seconds, Send/Request data from I2C Buck 

// other constants
#define RADIATOR_EXTREME_TEMP 45.0	// If radiator temperature is greater that this constant, then reduce power
#define MAXIMUM_PWM_HOT_WIRE 700.0 // Maximum PWM for hot wire. full range will give too much current, so limit it.
#define MODE_SWITCH_WINDOW_SEC 300 // 5-minute observation window for tendency-based mode switching
