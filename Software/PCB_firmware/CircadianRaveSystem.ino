#include "definitions.h"

// Global Variables
ProfileStruct_TypeDef profiles_arr[NUM_LED_STRIPS];
CommandStruct_TypeDef cmd_struct;
ValidCommandStruct_TypeDef commands[NUM_COMMANDS];
uint32_t time_last = 0;
uint8_t buffer_index = 0; // Index to keep track of the current position in the buffer
uint8_t led_status = 0;
uint8_t led_pins[6] = {LED1_PIN, LED2_PIN, LED3_PIN, LED4_PIN, LED5_PIN, LED6_PIN};
char input_buffer[RX_BUFFER_SIZE]; // Buffer to store the incoming string
float exp_lut[NUM_POINTS] = {0.00, 0.05, 0.10, 0.15, 0.20, 0.26, 0.32, 0.38, 0.45, 0.51, 0.59, 0.66, 0.74, 0.82, 0.91, 1.00, 1.09, 1.19, 1.29, 1.40, 1.52, 1.64, 1.76, 1.89, 2.03, 2.17, 2.32, 2.48, 2.64, 2.81, 2.99, 3.18, 3.38, 3.59, 3.80, 4.03, 4.27, 4.52, 4.78, 5.05, 5.33, 5.63, 5.95, 6.28, 6.62, 6.98, 7.36, 7.75, 8.16, 8.60, 9.05, 9.52, 10.02, 10.54, 11.09, 11.66, 12.26, 12.88, 13.54, 14.22, 14.94, 15.70, 16.49, 17.31, 18.18, 19.08, 20.03, 21.02, 22.06, 23.15, 24.29, 25.49, 26.74, 28.05, 29.42, 30.86, 32.36, 33.94, 35.59, 37.32, 39.13, 41.02, 43.01, 45.09, 47.26, 49.54, 51.93, 54.43, 57.05, 59.79, 62.66, 65.67, 68.82, 72.12, 75.57, 79.19, 82.97, 86.94, 91.09, 95.44, 100.00};


void setup() 
{
  Serial.begin(BAUDRATE);
  pwm_init();
  setup_valid_commands();
  
  #ifdef TEST
  Serial.print("TEST BUILD: DEBUGGING ON\n");
  Serial.print("---- Circadian Rave System ----\n");
  #endif

  // Create and initialize the profile structures
  for(int i=0; i<NUM_LED_STRIPS; i++)
  {
    profiles_arr[i] = init_profile_struct();
    profiles_arr[i].index = i + PROFILE_INDEX_START;
    profiles_arr[i].led_pin = led_pins[i];
  }

  // Setup Arduino LED Pins
  pinMode(13, OUTPUT);
  
}

void loop() 
{
  serial_read();

  // update the profiles that are running every one second.
  if (millis() - time_last > ONE_SECOND)
  {
    time_last = millis();
    //Serial.println(millis());

    // LED Flash every 1 second
    led_status = !led_status; // toggle status
    digitalWrite(13, led_status);
  
    // Go through each profile to update the data
    for(int i=0; i<NUM_LED_STRIPS; i++)
    {
      //only update the profiles that are running
      if (profiles_arr[i].profile_status == RUNNING)
      {
        update_profile(&profiles_arr[i]);
      }
    }
  }
}

// FUNCTIONS START HERE

void pwm_init(void)
{
  for(int i=0; i<NUM_LED_STRIPS; i++)
  {
    pinMode(led_pins[i], OUTPUT);
    analogWrite(led_pins[i], 0);
  }

  // Set Timer 0 (controls pins 5 and 6)
  TCCR0B =  (1 << CS01)|(1 << CS00); // Prescalar = 64. 16M/64/255 so PWM frequency is 980 Hz

  // Set Timer 1 (controls pins 9 and 10)
  // Timer 1 is a 16-bit timer so I am assuming it is setup as an 8 bit resolution
  TCCR1B = (1 << CS11)|(1 << CS10); // // Prescalar = 64 so PWM frequency is around 980 Hz
  
  // Set Timer 2 (controls pins 3 and 11)
  TCCR2B =  (1 << CS22); // Prescalar = 64 so PWM frequency is around 980 Hz
}

void update_profile(ProfileStruct_TypeDef* profile)
{
  uint32_t T1 = profile->time_vals[1];
  uint32_t T5 = profile->time_vals[5];
  uint32_t TEND = profile->time_vals[6];
  uint32_t T5_actual = T1 + (profile->num_cycles*(T5-T1));
  uint32_t time;

  // update the timer
  profile->timer_seconds += 1; 
  time = profile->timer_seconds;

  // update which step the profile is on
  if(time < T1)
  { // STEP 1
      profile->current_step = STEP1;
  }
  
  if(time >= T1 && time < T5_actual)
  { // STEP 2
      profile->current_step = STEP2;
      // update the current cycle variable as well.
      profile->current_cycle = 1 + ((time - T1)/(T5-T1));
  }

  if(time >= T5_actual)
  { // STEP 3
    profile->current_step = STEP3;
  }

  if(time >= TEND)
  {
    profile->current_step = COMPLETE;
  }

  // update the led intensity.
  update_led_intensity(profile);    
}

void update_led_intensity(ProfileStruct_TypeDef* profile)
{
  // function looks at the profile and calculates the LED intensity by linear interpolation
  uint32_t T1 = profile->time_vals[1];
  uint32_t T5 = profile->time_vals[5];
  uint32_t time = profile->timer_seconds;
  uint16_t pwm_duty = 0;
  uint32_t x = 0;  // time point based on where in the cycle we are

  if(profile->current_step == STEP1)
  {
    x = time;
    profile->current_intensity = map(x, profile->time_vals[0], profile->time_vals[1], profile->intensity_vals[0], profile->intensity_vals[1]);
    if (profile->curve == CURVE_LINEAR)
    {
      pwm_duty = map(profile->current_intensity, 0, 100, 0, PWM_TOP); 
    }
    if (profile->curve == CURVE_EXPONENTIAL)
    {
      profile->current_intensity  = exp_lut[profile->current_intensity];
      pwm_duty = map(profile->current_intensity, 0, 100, 0, PWM_TOP);
    }
    analogWrite(profile->led_pin, pwm_duty);
  }

  if(profile->current_step == STEP3)
  {
    x = time - T5;
    profile->current_intensity = map(x, profile->time_vals[5], profile->time_vals[6], profile->intensity_vals[5], profile->intensity_vals[6]);
    if (profile->curve == CURVE_LINEAR)
    {
      pwm_duty = map(profile->current_intensity, 0, 100, 0, PWM_TOP); 
    }
    if (profile->curve == CURVE_EXPONENTIAL)
    {
      profile->current_intensity  = exp_lut[profile->current_intensity];
      pwm_duty = map(profile->current_intensity, 0, 100, 0, PWM_TOP);
    }
    analogWrite(profile->led_pin, pwm_duty);
  }

  // If the profile is complete, then turn LED off.
  if(profile->current_step == COMPLETE)
  {
    profile->current_intensity = 0;
    analogWrite(profile->led_pin, 0);
  }

  if(profile->current_step == STEP2)
  {
    x = ((time - T1) % (T5-T1)) + T1;
    
    // using the timepoint x, find the index where it falls
    uint8_t i = 0;
    for (i = 0; i < NUM_TIME_VAL_PAIRS; i++) {
        if (x <= profile->time_vals[i+1]) {
            break;
        }
    }
    
    profile->current_intensity = map(x, profile->time_vals[i], profile->time_vals[i+1], profile->intensity_vals[i], profile->intensity_vals[i+1]);
    if (profile->curve == CURVE_LINEAR)
    {
      pwm_duty = map(profile->current_intensity, 0, 100, 0, PWM_TOP);
    } 
    if (profile->curve == CURVE_EXPONENTIAL)
    {
      profile->current_intensity  = exp_lut[profile->current_intensity];
      pwm_duty = map(profile->current_intensity, 0, 100, 0, PWM_TOP);
    }
    analogWrite(profile->led_pin, pwm_duty);
  }  

}

// Function to check if an array is monotonically increasing
bool is_increasing(uint32_t* array, int size)
{
  bool increasing = true;
  if (size <= 1) 
  {
    return true; // An array with 0 or 1 elements is monotonic by definition
  }
  for (int i = 1; i < size; ++i) 
  {
    if (array[i] <= array[i - 1]) 
    {
      increasing = false;
    }
  }
  return increasing;
}

void send_packet(ProfileStruct_TypeDef* profiles)
{
  //"status, time, num_cycles, current_step, current_cycle, current_intensity, curve
  for(int i=0; i<NUM_LED_STRIPS; i++)
  { 
    Serial.print("{P");
    Serial.print(profiles[i].index);
    Serial.print(",");
    Serial.print(profiles[i].profile_status);
    Serial.print(",");
    Serial.print(profiles[i].timer_seconds);
    Serial.print(",");
    Serial.print(profiles[i].num_cycles);
    Serial.print(",");
    Serial.print(profiles[i].current_step);
    Serial.print(",");
    Serial.print(profiles[i].current_cycle);
    Serial.print(",");
    Serial.print(profiles[i].current_intensity);
    Serial.print(",");
    Serial.print(profiles[i].curve);
    Serial.print(",[");

    for(int j=0; j<NUM_TIME_VAL_PAIRS; j++)
    {
        Serial.print(profiles[i].time_vals[j]);
        if(j<NUM_TIME_VAL_PAIRS-1){
          Serial.print(",");
        }
    }
    Serial.print("],[");

    for(int j=0; j<NUM_TIME_VAL_PAIRS; j++)
    {
        Serial.print(profiles[i].intensity_vals[j]);
        if(j<NUM_TIME_VAL_PAIRS-1){
          Serial.print(",");
        }
    }
    Serial.print("]},");
    #ifdef TEST
      Serial.print("]}\n");
    #else
      Serial.print("]},");
    #endif
  }
  Serial.println("PEACE-OUT!");

}

void valid_command_response(CommandStruct_TypeDef* cmd_struct)
{
  #ifdef TEST
  Serial.print(float(millis()/1000.0), 2);
  Serial.print("s -> COMMAND OK: ");
  Serial.print(cmd_struct->cmd);
  Serial.print(" ");
  Serial.print(cmd_struct->profile_index); 
  Serial.print(" ");
  Serial.print(cmd_struct->val1);
  Serial.print(" ");
  Serial.print(cmd_struct->val2);
  Serial.print("\n");
  send_packet(profiles_arr);  // THIS IS TEMPORARY
  #endif
}


uint8_t get_profile_index(CommandStruct_TypeDef* cmd_struct)
{
  // Returns the profile index based on the c indexing scheme. 
  // Profile 1 might be index 0, so it returns 0
  return (cmd_struct->profile_index) - PROFILE_INDEX_START;
}

uint8_t get_time_index(CommandStruct_TypeDef* cmd_struct)
{
  // returns the time index. So T1 is index 0
  uint8_t time_index;
  sscanf(cmd_struct->cmd, "set_T%d", &time_index);
  //time_index = time_index - 1;
  return time_index;
}

void invalid_command_response(char* response)
{
  #ifdef TEST
  Serial.print((float)(millis()/1000.0), 2);
  Serial.print("s -> BAD COMMAND: ");
	Serial.println(response);
  #endif
}

void process_command(CommandStruct_TypeDef* cmd_struct, ProfileStruct_TypeDef* profiles, char* buffer)
{
  // this function is called once the command has been validated and we need to execute the commands
  uint8_t time_index;
  uint8_t profile_index;
  
  if (strcmp(cmd_struct->cmd, CMD_GET_STATUS) == 0)
  {
    #ifndef TEST  // during TEST builds, print out the packet when there is a valid command
    send_packet(profiles);
    #endif
    valid_command_response(cmd_struct);
  }

  if (strcmp(cmd_struct->cmd, CMD_START) == 0)
  {
    profile_index = get_profile_index(cmd_struct);

    if(profile_index>=0 && profile_index < NUM_LED_STRIPS)
    {
      // check increasing time values
      if(is_increasing(profiles[profile_index].time_vals, NUM_TIME_VAL_PAIRS))
      {
        profiles[profile_index].profile_status = RUNNING;
        valid_command_response(cmd_struct);
      }
      else
      {
        #ifdef TEST
        Serial.println("ERROR: NON-MONOTIC TIME VALS");
        #endif
      }
    }
    else
    {
      #ifdef TEST
      invalid_command_response(buffer);
      Serial.println("ERROR: Profile Index Out of Range");
      #endif
    }
  }

  if (strcmp(cmd_struct->cmd, CMD_STOP) == 0)
  {
    profile_index = get_profile_index(cmd_struct);
    profiles[profile_index].profile_status = NOT_STARTED;
    profiles[profile_index].timer_seconds = 0;    // reset time value to 0
    profiles[profile_index].current_step = STEP1;
    profiles[profile_index].current_cycle = 0;
    profiles[profile_index].current_intensity = 0;
    analogWrite(profiles[profile_index].led_pin, 0); // reset LED intensity to 0
    valid_command_response(cmd_struct);
  }

  if (strcmp(cmd_struct->cmd, SET_CYCLES) == 0)
  {
    profile_index = get_profile_index(cmd_struct);
    profiles[profile_index].num_cycles = cmd_struct->val1;
    valid_command_response(cmd_struct);
  }

  // TODO - placeholder for now.
  if (strcmp(cmd_struct->cmd, SET_CURVE) == 0)
  {
    profile_index = get_profile_index(cmd_struct);
    profiles[profile_index].curve = cmd_struct->val1;
    valid_command_response(cmd_struct);
  }

  // Set the Time Profiles
  char dest[6]; // Allocate enough space for 5 characters + null terminator
  strncpy(dest, cmd_struct->cmd, 5); // Copy the first 5 characters from cmd_struct->cmd to dest
  dest[5] = '\0'; //// Null-terminate the destination string

  if (strcmp(dest, "set_T") == 0)
  {
    // first 5 chars are SET_T so now see which T number it is
    time_index = get_time_index(cmd_struct);
    profile_index = get_profile_index(cmd_struct);

    if(profile_index >=0 && profile_index < NUM_LED_STRIPS)
    {
      if(time_index >=0 && time_index < NUM_TIME_VAL_PAIRS)
      {
        profiles[profile_index].time_vals[time_index] = cmd_struct->val1;
        profiles[profile_index].intensity_vals[time_index] = cmd_struct->val2;
        valid_command_response(cmd_struct);
      }
    }
  }

  if (strcmp(cmd_struct->cmd, RESET_PROFILES) == 0)
  {
    reset_profiles();
    valid_command_response(cmd_struct);
  }


}

void serial_read()
{

  while (Serial.available() > 0) 
  {
    char incoming_byte = Serial.read(); // Read the incoming byte

    // Check for end of line character (newline)
    if (incoming_byte == '\n') 
    {
			command_reset(&cmd_struct); // reset the command structure and parse the input buffer into the command struct
      sscanf(input_buffer, "%s %d %lu %lu\n", &cmd_struct.cmd, &cmd_struct.profile_index, &cmd_struct.val1, &cmd_struct.val2);
      //sscanf(input_buffer, "%s %d %d %d\n", &cmd_struct.cmd, &cmd_struct.profile_index, &cmd_struct.val1, &cmd_struct.val2);

      // Digest Command and only print if it was valid
			if (command_digest(&cmd_struct) == CMD_OK)
			{
        process_command(&cmd_struct, profiles_arr, input_buffer);
			}
			else
			{
        invalid_command_response(input_buffer);
			}

      buffer_index = 0; // Reset the buffer index for the next string
      memset(input_buffer, '\0', sizeof(input_buffer)); // empty the buffer
    } 
    else 
    {
      // Add the incoming byte to the buffer if there is space
      if (buffer_index < RX_BUFFER_SIZE - 1) 
      {
        input_buffer[buffer_index] = incoming_byte;
        buffer_index++;
      }
    }
  }

}

void reset_profiles()
{
  for (int i=0; i<NUM_LED_STRIPS; i++)
  {
    profiles_arr[i] = init_profile_struct();
  }
  
}


ProfileStruct_TypeDef init_profile_struct()
{
  ProfileStruct_TypeDef profile;
  profile.index = 0;
  profile.timer_seconds = 0;
  profile.led_pin = 0;
  profile.num_cycles = 1;
  profile.current_cycle = 0;
  profile.curve = CURVE_LINEAR;
  profile.current_step = 1;
  profile.profile_status = NOT_STARTED;
  profile.current_intensity = 0;

  for (int i=0; i<NUM_TIME_VAL_PAIRS; i++)
  {
    profile.time_vals[i] = 0;
    profile.intensity_vals[i] = 0;
  }

  profile.time_vals[0] = 0;
  profile.time_vals[1] = DEFAULT_TSRART_DURATION;
  profile.time_vals[2] = DEFAULT_T1_DURATION + profile.time_vals[1];
  profile.time_vals[3] = DEFAULT_T2_DURATION + profile.time_vals[2];
  profile.time_vals[4] = DEFAULT_T3_DURATION + profile.time_vals[3];
  profile.time_vals[5] = DEFAULT_T4_DURATION + profile.time_vals[4];
  profile.time_vals[6] = DEFAULT_TEND_DURATION + profile.time_vals[5];

  profile.intensity_vals[2] = 100;
  profile.intensity_vals[3] = 100;

  return profile;
}

void command_reset(CommandStruct_TypeDef* cmd_struct)
{
  memset(cmd_struct->cmd, '\0', sizeof(cmd_struct->cmd)); 
  cmd_struct->profile_index = 0;
	cmd_struct->val1 = 0;
	cmd_struct->val2 = 0;
}

void setup_valid_commands()
{
  sprintf(commands[0].cmd, "%s", CMD_GET_STATUS);
  sprintf(commands[1].cmd, "%s", CMD_START);
  sprintf(commands[2].cmd, "%s", CMD_STOP);

  sprintf(commands[3].cmd, "%s", SET_CYCLES);
  sprintf(commands[4].cmd, "%s", SET_CURVE);
  sprintf(commands[5].cmd, "%s", SET_T0);
  sprintf(commands[6].cmd, "%s", SET_T1);
  sprintf(commands[7].cmd, "%s", SET_T2);
  sprintf(commands[8].cmd, "%s", SET_T3);
  sprintf(commands[9].cmd, "%s", SET_T4);
  sprintf(commands[10].cmd, "%s", SET_T5);
  sprintf(commands[11].cmd, "%s", SET_T6);
  sprintf(commands[12].cmd, "%s", RESET_PROFILES);

}

uint8_t command_digest(CommandStruct_TypeDef* cmd_struct)
{
	uint8_t return_val = CMD_ERROR; // default output is an error unless it is overriden

  // compare against the command strings
  for(int i=0; i<NUM_COMMANDS; i++)
  {
    if (strcmp(cmd_struct->cmd, commands[i].cmd) == 0)
    {
      return_val = CMD_OK;
    }
  }
  return return_val;
}
