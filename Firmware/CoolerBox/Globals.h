// Debugging options
//#define WIFI_DEBUG
//#define STALL_DEBUG
//#define DEBUG_TEMPERATURE_SENSORS
#define ENABLE_PID_FAST_DEBUG

// Default values
#define DEFAULT_HYSTERESIS 0.05  // ±0.05 °C (0.10 °C total deadband span)
#define DEFAULT_PID_KP 10.0f
#define DEFAULT_PID_KI 0.035f
#define DEFAULT_PID_KD 1.5f

#define PIDCOMPUTE_INTERVAL 2	// seconds, continuous 12-bit DS18B20 conversion
#define I2C_INTERVAL 2			// seconds, Send/Request data from I2C Buck 

// other constants
#define RADIATOR_EXTREME_TEMP 45.0	// If radiator temperature is greater that this constant, then reduce power
#define MAXIMUM_PWM_HOT_WIRE 500.0 // Maximum PWM for hot wire. full range will give too much current, so limit it.
#define MODE_SWITCH_WINDOW_SEC 300 // 5-minute observation window for tendency-based mode switching
