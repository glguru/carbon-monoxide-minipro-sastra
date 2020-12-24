
int time_scale = 8;

void setTimer0PWM(byte chA, byte chB) //pins D5 and D6
{
  TCCR0A = 0b10110011; //OCA normal,OCB inverted, fast pwm
  TCCR0B = 0b010; //8 prescaler - instead of system's default 64 prescaler - thus time moves 8 times faster
  OCR0A = chA; //0..255
  OCR0B = chB;
}


void setTimer2PWM(byte chA, byte chB) //pins D11 and D3
{
  TCCR2A = 0b10100011; //OCA,OCB, fast pwm
  TCCR2B = 0b001; //no prescaler
  OCR2A = chA; //0..255
  OCR2B = chB;
}

void setTimer1PWM(int chA, int chB) //pins D9 and D10 (LED)
{
  TCCR1A = 0b10100011; //OCA,OCB, fast pwm
  TCCR1B = 0b1001; //no prescaler
  OCR1A = chA; //0..1023
  OCR1B = chB;
}

float opt_voltage = 0;
byte opt_width = 240; //default reasonable value


void pwm_adjust()
{
  float previous_v = 5.0; //voltage at previous attempt
  float raw2v = 5.0 / 1024.0;//coefficient to convert Arduino's   (ADC)
  //analogRead result into voltage in volts
  for(int w = 0; w < 250; w++)
  {
    setTimer2PWM(0, w);
    float avg_v = 0;
    for(int x = 0; x < 100; x ++) //measure over about 100ms to ensure stable result
    {
      avg_v += analogRead(A1);
      delay(time_scale);
    }
    avg_v *= 0.01;
    avg_v *= raw2v;
    Serial.print("adjusting PWM w=");
    Serial.print(w);
    Serial.print(", V=");
    Serial.println(avg_v);
    if(avg_v < 3.6 && previous_v > 3.6) //we found optimal width
    {
      float dnew = 3.6 - avg_v; //now we need to find if current one
      float dprev = previous_v - 3.6;//or previous one is better
      if(dnew < dprev) //if new one is closer to 1.4 then return it
      {
        opt_voltage = avg_v;
        opt_width = w;
        return;
      }
      else //else return previous one
      {
        opt_voltage = previous_v;
        opt_width = w-1;
        return;
      }
    }
    previous_v = avg_v;
    opt_voltage=avg_v;
    opt_width=w;
  }
}

float red_threshold = 10; 
float reference_resistor_kOhm = 10.0; 

float sensor_reading_clean_air = 250; 
float sensor_reading_100_ppm_CO = -1; 

float sensor_100ppm_CO_resistance_kOhm; //calculated from sensor_reading_100_ppm_CO variable
float sensor_base_resistance_kOhm; //calculated from sensor_reading_clean_air variable

byte phase = 0; //1 - high voltage, 0 - low voltage
unsigned long prev_ms = 0;
unsigned long sec10 = 0; 
//please take care of overflow problem 
unsigned long high_on = 0; //time when we started high temperature cycle
unsigned long low_on = 0; //time when we started low temperature cycle
unsigned long last_print = 0; //time when we last printed message in serial

float sens_val = 0;
float last_CO_ppm_measurement = 0; 

float raw_value_to_CO_ppm(float value)
{
  if(value < 1) return -1; //wrong input value
  sensor_base_resistance_kOhm = reference_resistor_kOhm * 1023 / sensor_reading_clean_air - reference_resistor_kOhm;    //  30.92
  if(sensor_reading_100_ppm_CO > 0)
  {
    sensor_100ppm_CO_resistance_kOhm = reference_resistor_kOhm * 1023 / sensor_reading_100_ppm_CO - reference_resistor_kOhm;
  }
  else
  {
    sensor_100ppm_CO_resistance_kOhm = sensor_base_resistance_kOhm * 0.25;   // Ro= Rs x 0.25 = 7.73
  }
  float sensor_R_kOhm = reference_resistor_kOhm * 1023 / value - reference_resistor_kOhm;           // (10230/val) - 10  = 24.1   assumig val =300
  float R_relation = sensor_100ppm_CO_resistance_kOhm / sensor_R_kOhm;                              //  7.73/  ^   = 7.73/24.1  =  0.320 
  float CO_ppm = 134 * R_relation - 35;                                                             //  134 * 0.32 -35 = 7.9
  if(CO_ppm < 0) CO_ppm = 0;
  return CO_ppm;
}

void startMeasurementPhase()
{
  phase = 0;
  low_on = sec10;
  setTimer2PWM(0, opt_width);
}

void startHeatingPhase()
{
  phase = 1;
  high_on = sec10;
  setTimer2PWM(0, 255);
}
void setLEDs(int br_green, int br_red)
{
  if(br_red < 0) br_red = 0;
  if(br_red > 100) br_red = 100;
  if(br_green < 0) br_green = 0;
  if(br_green > 100) br_green = 100;

  float br = br_red;
  br *= 0.01;
  br = (exp(br)-1) / 1.7183 * 1023.0;
  float bg = br_green;
  bg *= 0.01;
  bg = (exp(bg)-1) / 1.7183 * 1023.0;
  if(br < 0) br = 0;
  if(br > 1023) br = 1023;
  if(bg < 0) bg = 0;
  if(bg > 1023) bg = 1023;

  setTimer1PWM(1023-br, 1023-bg);
}


void setup() {

  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  setTimer1PWM(1023, 0);
  analogReference(DEFAULT);
  Serial.begin(9600);

  pwm_adjust();

  Serial.print("PWM result: width ");
  Serial.print(opt_width);
  Serial.print(", voltage ");
  Serial.println(opt_voltage);
  Serial.println("Data output: raw A0 value, heating on/off (0.1 off 1000.1 on), CO ppm from last measurement cycle");

  startMeasurementPhase(); //start from measurement
}

void loop() 
{
  unsigned long ms = millis();
  int dt = ms - prev_ms;

  if(dt > 100*time_scale || dt < 0) 
  {
    prev_ms = ms; 
    sec10++; //increase 0.1s counter
    if(sec10%10 == 1) //LEDs to blink periodically
    {
      setTimer1PWM(1023, 1023); //blink LEDs once per second
    }
    else //calculate LEDs state
    {
      int br_red = 0, br_green = 0; //brightness from 1 to 100, setLEDs function handles converting it into timer settings
      if(last_CO_ppm_measurement <= red_threshold) //turn green LED if we are below 30 PPM
      {//the brighter it is, the lower concentration is
        br_red = 0; //turn off red
        br_green = (red_threshold - last_CO_ppm_measurement)*100.0/red_threshold; //the more negative is concentration, the higher is value
        if(br_green < 1) br_green = 1; //don't turn off completely
      }
      else //if we are above threshold, turn on red one
      {
        br_green = 0; //keep green off
        br_red = (last_CO_ppm_measurement-red_threshold)*100.0/red_threshold; //the higher is concentration, the higher is this value
        if(br_red < 1) br_red = 1; //don't turn off completely
      }
      setTimer0PWM(255, 255);

      setLEDs(br_green, br_red); //set LEDs brightness
    }
  }
  if(phase == 1 && sec10 - high_on > 600) //60 seconds of heating ended?
    startMeasurementPhase();
  if(phase == 0 && sec10 - low_on > 900) //90 seconds of measurement ended?
  {
    last_CO_ppm_measurement = raw_value_to_CO_ppm(sens_val);
    startHeatingPhase();
  }

  float v = analogRead(A0); //reading value
  sens_val *= 0.999; //applying exponential averaging using formula
  sens_val += 0.001*v; // average = old_average*a + (1-a)*new_reading
  if(sec10 - last_print > 9) //print measurement result into serial 2 times per second
  {
    last_print = sec10;
    Serial.print(sens_val);
    Serial.print("x");
    Serial.print(0.1 + phase*1000);
    Serial.print("x");
    Serial.println(last_CO_ppm_measurement);
  }
}
