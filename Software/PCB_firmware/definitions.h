#define BAUDRATE 115200

//#define TEST // comment out if in production

#define ONE_SECOND 999

// Pins
#define LED1_PIN  3
#define LED2_PIN  5
#define LED3_PIN  6
#define LED4_PIN  9
#define LED5_PIN  10
#define LED6_PIN  11


#define STEP1   1
#define STEP2   2
#define STEP3   3
#define COMPLETE 4


#define PWM_TOP 255

// Profile Status definitions
#define NOT_STARTED 0
#define RUNNING 1

#define RX_BUFFER_SIZE 70

#define CURVE_LINEAR 0
#define CURVE_EXPONENTIAL 1
#define NUM_POINTS 101

// Command Status definitions
#define CMD_OK 1        // command received ok
#define CMD_ERROR 100   // command error

#define NUM_TIME_VAL_PAIRS 7
#define NUM_LED_STRIPS 6

#define PROFILE_INDEX_START 1 // Either 0 or 1

#define NUM_COMMANDS 13

#define CMD_GET_STATUS  "get_status"
#define CMD_START       "start"
#define CMD_STOP        "stop"
#define SET_CYCLES      "set_cycles"
#define SET_CURVE       "set_curve"
#define SET_T0          "set_T0"
#define SET_T1          "set_T1"
#define SET_T2          "set_T2"
#define SET_T3          "set_T3"
#define SET_T4          "set_T4"
#define SET_T5          "set_T5"
#define SET_T6          "set_T6"
#define RESET_PROFILES  "reset_profiles"

#define DEFAULT_TSRART_DURATION 10
#define DEFAULT_T1_DURATION 10
#define DEFAULT_T2_DURATION 10
#define DEFAULT_T3_DURATION 10
#define DEFAULT_T4_DURATION 10
#define DEFAULT_TEND_DURATION 10

typedef struct Profile{
  uint8_t index;
  uint32_t timer_seconds;
  uint8_t led_pin;
  uint8_t profile_status;
  uint16_t num_cycles;
  uint16_t current_cycle;
  uint8_t current_step;
  uint8_t curve;
  uint8_t current_intensity;
  uint32_t time_vals[NUM_TIME_VAL_PAIRS];
  uint8_t intensity_vals[NUM_TIME_VAL_PAIRS];
} ProfileStruct_TypeDef;

typedef struct Command{
  char cmd[15];
  uint8_t profile_index;
  uint32_t val1;
  uint32_t val2;
} CommandStruct_TypeDef;

typedef struct ValidCommands{
  char cmd[15];
} ValidCommandStruct_TypeDef;


// Function declerations
void pwm_init(void);
ProfileStruct_TypeDef init_profile_struct();
void command_reset(CommandStruct_TypeDef* cmd_struct);
void process_command(CommandStruct_TypeDef* cmd_struct, ProfileStruct_TypeDef* profiles, char* buffer);
void send_packet(ProfileStruct_TypeDef* profiles);
void serial_read(void);
uint8_t command_digest(CommandStruct_TypeDef* cmd_struct);
void setup_valid_commands();
void valid_command_response(CommandStruct_TypeDef* cmd_struct);
uint8_t get_profile_index(CommandStruct_TypeDef* cmd_struct);
uint8_t get_time_index(CommandStruct_TypeDef* cmd_struct);
bool is_increasing(uint32_t* array, int size);
void update_led_intensity(ProfileStruct_TypeDef* profile);
void update_profile(ProfileStruct_TypeDef* profile);
void reset_profiles(void);
