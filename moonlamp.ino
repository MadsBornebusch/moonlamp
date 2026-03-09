#include <avr/io.h>
#include <avr/interrupt.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include "RV3032.h"

#define MAIN_STATE_EEPROM 0
#define COLOR_STATE_EEPROM 1
#define CUSTOM_COLOR_EEPROM 2
#define BRIGHTNESS_EEPROM 3
#define ORANGE_HOUR_EEPROM 4
#define ORANGE_MINUTE_EEPROM 5
#define MORNING_HOUR_EEPROM 6
#define MORNING_MINUTE_EEPROM 7



RV3032 rtc;


#define PIN_NEOPIXEL 10
#define NUM_LEDS 14
Adafruit_NeoPixel leds(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Global display data - 8MHz version
volatile uint8_t dig[4], comma, clearDig;
volatile uint8_t curDigit = 0;

// Patterns for digits and some characters
const uint8_t pat[] = {
  0x3F, // 0
  0x06, // 1
  0x5B, // 2
  0x4F, // 3
  0x66, // 4
  0x6D, // 5
  0x7D, // 6
  0x07, // 7
  0x7F, // 8
  0x6F, // 9
  0x77, // A
  0x7C, // B
  0x39, // C
  0x5E, // D
  0x79, // E
  0x71, // F
  0x3D, // G
  0x76, // H
  0x30, // I
  0x1E, // J
  0x75, // K (alternative pattern)
  0x38, // L
  0x55, // M (custom pattern)
  0x54, // N
  0x3F, // O (same as 0)
  0x73, // P
  0x6B, // Q (custom pattern)
  0x50, // R (custom pattern)
  0x6D, // S (same as 5)
  0x78, // T
  0x3E, // U
  0x3E, // V (same as U)
  0x2A, // W (custom pattern)
  0x76, // X (same as H)
  0x6E, // Y
  0x5B  // Z
};


const uint8_t PROGMEM gamma8[] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };



// Button handling variables
#define DEBOUNCE_MS 50
volatile uint8_t btn[3] = {0};
volatile uint8_t btn_state[3] = {0xFF};
volatile uint32_t t0=0, t1=0, t2=0;

// Display function - shows a 4-digit number with comma control and character disable functionality
void disp(uint16_t digit, uint8_t c = 0, uint8_t charDis = 0) {
  dig[0]=digit%10;
  dig[1]=(digit/10)%10;
  dig[2]=(digit/100)%10;
  dig[3]=(digit/1000)%10;
  comma=c&0x0F;
  clearDig = charDis;
}

// Function to map a character to its pattern index
uint8_t charToIndex(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0'; // Map '0'-'9' to indices 0-9
  } else if (c >= 'a' && c <= 'z') {
    return c - 'a' + 10; // Map 'a'-'z' to indices 10-35
  }
  return 0; // Default to 0 if character is not found
}

// Display function - shows characters with comma control and character disable functionality
void dispChar(const char* text, uint8_t c = 0, uint8_t charDis = 0) {
  uint8_t len = strlen(text);
  for (uint8_t i = 0; i < 4; i++) {
    if (i < len) {
      // Calculate the index from the end of the string
      uint8_t text_index = len - 1 - i;
      char current_char = text[text_index];
      dig[i] = charToIndex(current_char);
    } else {
      dig[i] = 0; // Default to 0 if no character is provided
    }
  }
  comma = c & 0x0F;
  clearDig = charDis;
}

void dispTime(uint8_t hour, uint8_t minute) {
  disp(hour*100+minute,0b0100);
}


// Timer setup for display multiplexing - 2kHz at 8MHz
void setupTimer() {
  TCCR1A=0;
  TCCR1B=(1<<WGM12)|(1<<CS11)|(1<<CS10);  // Prescaler 64
  OCR1A=249;  // 8MHz/64/250 = 500Hz
  TIMSK1|=(1<<OCIE1A);
}

// Display interrupt - multiplexes 4 digits
ISR(TIMER1_COMPA_vect) {
  PORTC&=~0x0F;  // Digits off

  uint8_t p=pat[dig[curDigit]];
  if(comma&(1<<curDigit)) p|=0x80;
  //Clear segment enable pins, and set comma segment pin
  PORTD = (PORTD&0x0F)|(p&0x80);
  PORTB = (PORTB&0x3C);
  // If digit is "clear" dont set the segment pins
  if(!(clearDig&(1<<curDigit))) {
    PORTD|=((p&0x01)<<4)|((p&0x04)<<4)|((p&0x40)>>1);
    PORTB|=((p&0x02)<<6)|((p&0x08)>>3)|((p&0x10)>>3)|((p&0x20)<<1);
  }
  PORTC|= (1<<curDigit);  // Digit on
  curDigit=(curDigit+1)&3;
}


// Button interrupt handlers
// Single ISR for PCINT2 (handles both PCINT16 and PCINT17)
ISR(PCINT2_vect) {
  uint32_t now = millis();
  uint8_t current_state_0 = (PIND & (1<<1)) ? 1 : 0;
  // If state changed from HIGH to LOW (falling edge) and debounced
  if ((current_state_0 == 0) && (btn_state[0] == 1) && (now - t0 > DEBOUNCE_MS)) {
    ++btn[0];
    t0 = now;
  }
  btn_state[0] = current_state_0;
    uint8_t current_state_2 = (PIND & (1<<0)) ? 1 : 0;
  // If state changed from HIGH to LOW (falling edge) and debounced
  if ((current_state_2 == 0) && (btn_state[2] == 1) && (now - t2 > DEBOUNCE_MS)) {
    ++btn[2];
    t2 = now;
  }
  btn_state[2] = current_state_2;
}

// INT1 ISR for button 1
ISR(INT1_vect) { 
  uint32_t now = millis();
  if (now - t1 > DEBOUNCE_MS) { btn[1]++; t1=now; }
}

void enableButtonInterrupts () {
  DDRD &= ~0x0B;        // PD0,1,3 as inputs
  EICRA = (1<<ISC11);   // INT1 falling edge
  EIMSK = (1<<INT1);    // Enable INT1
  PCICR = (1<<PCIE2);   // Enable PCINT2
  PCMSK2 = 0x03;        // PCINT16 & PCINT17
  sei();                // Enable interrupts
}

void displayPortSetup() {
  DDRD|=(1<<4)|(1<<5)|(1<<6)|(1<<7);
  DDRB|=(1<<0)|(1<<1)|(1<<6)|(1<<7);
  DDRC|=0x0F;
  PORTC&=~0x0F;
}

uint32_t colorWheel(uint8_t WheelPos) {
  WheelPos = 255 - WheelPos;
  uint8_t red, green, blue;
  if(WheelPos < 85) {
    red = pgm_read_byte(gamma8 + (255 - WheelPos * 3));
    green = 0;
    blue = pgm_read_byte(gamma8 + (WheelPos * 3));
  }else if(WheelPos < 170) {
    WheelPos -= 85;
    red = 0;
    green = pgm_read_byte(gamma8 + (WheelPos * 3));;
    blue = pgm_read_byte(gamma8 + (255 - WheelPos * 3));
  } else {
    WheelPos -= 170;
    red = pgm_read_byte(gamma8 + (WheelPos * 3));
    green = pgm_read_byte(gamma8 + (255 - WheelPos * 3));
    blue = 0;
  }
  return leds.Color(red, green, blue);
}

int16_t wrapAround(int16_t value, int16_t min, int16_t max) {
  int16_t range = max - min + 1;
  return ((value - min) % range + range) % range + min;
}

enum main_state_t {
  OFF,
  ON,
  ADJUSTING
};

enum adjustment_state_t {
  ORANGE_HOUR,
  ORANGE_MINUTE,
  MORNING_HOUR,
  MORNING_MINUTE,
  YEAR,
  MONTH,
  DAY,
  DOW,
  TIME_HOUR,
  TIME_MINUTE
};

enum adjustment_substate_t {
  ENTER_ADJUSTMENT,
  FLASH_NAME,
  SHOW_VAL,
  HIDE_VAL
};

enum color_state_t {
  WARM_WHITE = 0,
  WHITE = 1,
  CUSTOM_SELECTION = 2,
  CUSTOM_SELECTED = 3,
  NUM_STATES = 4
};
main_state_t main_state;
adjustment_state_t adjustment_state;
adjustment_substate_t adjustment_substate;
color_state_t color_state;
uint8_t custom_color_wheel_index;
uint32_t custom_color;
uint8_t brightness;

uint8_t orange_hour;
uint8_t orange_minute;
uint8_t morning_hour;
uint8_t morning_minute;
struct tm timeinfo = {
    .tm_sec = 0,            // Second (0-60)
    .tm_min = 30,           // Minute (0-59)
    .tm_hour = 12,          // Hour (0-23)
    .tm_mday = 5,           // Day of the month (1-31)
    .tm_wday = 0,           // Day of the week (0-6, Sunday is 0)
    .tm_mon = 1,            // Month (0-11)
    .tm_year = 2026 - 1900, // Year since 1900
    .tm_yday = 5,
    .tm_isdst = 0
};


void setup() {
  // Setup display ports
  displayPortSetup();
  // Setup timer and button interrupts
  setupTimer();
  enableButtonInterrupts();
  leds.begin();
  leds.clear();
  leds.show();

  rtc.begin();
  rtc.getTime(timeinfo);
  // Initial display test, show year
  dispChar("year");
  delay(500);
  disp(timeinfo.tm_year + 1900);
  delay(1000);
  dispChar("time");
  delay(500);
  disp(timeinfo.tm_hour * 100 + timeinfo.tm_min);
  delay(1000);
  disp(0,0,0xf);  // Clear display
  main_state = OFF;
  color_state = constrain(EEPROM.read(COLOR_STATE_EEPROM),0,2);
  custom_color_wheel_index = EEPROM.read(CUSTOM_COLOR_EEPROM);
  brightness = constrain(EEPROM.read(BRIGHTNESS_EEPROM),1,5);
  orange_hour = constrain(EEPROM.read(ORANGE_HOUR_EEPROM),0,23);
  orange_minute = constrain(EEPROM.read(ORANGE_MINUTE_EEPROM),0,59);
  morning_hour = constrain(EEPROM.read(MORNING_HOUR_EEPROM),0,23);
  morning_minute = constrain(EEPROM.read(MORNING_MINUTE_EEPROM),0,59);
}


uint8_t brightnessAdjust(uint8_t colorChannel) {
    switch(brightness) {
        case 1:
            return (colorChannel * 10 + 50) / 100;
        case 2:
            return (colorChannel * 10 + 50) / 100;
        case 3:
            return (colorChannel * 20 + 50) / 100;
        case 4:
            return (colorChannel * 60 + 50) / 100;
        default:
            return colorChannel; // Default case if brightness is not 1-4
    }
}

void setColor() {
  if(main_state != ON) {
    leds.clear();
  } else {
    uint32_t color;
    if(color_state == WARM_WHITE) {
      color = leds.Color(brightnessAdjust(255), brightnessAdjust(120), brightnessAdjust(30));
    } else if (color_state == WHITE) {
      color = leds.Color(brightnessAdjust(255), brightnessAdjust(255), brightnessAdjust(255));
    } else {
      uint32_t colorWheelColor = colorWheel(custom_color_wheel_index);
      uint8_t red = (colorWheelColor >> 16) & 0xFF;
      uint8_t green = (colorWheelColor >> 8) & 0xFF;
      uint8_t blue = colorWheelColor & 0xFF;
      color = leds.Color(brightnessAdjust(red), brightnessAdjust(green), brightnessAdjust(blue));
    }
    uint16_t pattern;
    if (brightness == 1)
        pattern = (1 << 0) | (1 << 3) | (1 << 7) | (1 << 10);  // LEDs 0,3,7,10
    else if (brightness == 2)
        pattern = 0x1555;  // even LEDs: 0,2,4,6,8,10,12 (binary 01010101010101)
    else
        pattern = 0xFFFF;  // all LEDs on for any other brightness

    for (int i = 0; i < NUM_LEDS; ++i) {
        leds.setPixelColor(i, (pattern >> i) & 1 ? color : 0);
    }
  }
  leds.show();
}

#include <time.h>




bool update = 1;
bool rotate_color = 0;
uint32_t last_rotation_time = millis();
uint32_t iteration_millis;
uint32_t entered_adjustment;
uint32_t time_diff;
bool long_press = 0;
uint32_t last_time_sample = 0;
int8_t adjustment_diff = 0;
bool auto_on = 0;
void loop() {
  iteration_millis = millis();


  if(main_state == ADJUSTING) {
    time_diff = iteration_millis - entered_adjustment;
    if((adjustment_substate == ENTER_ADJUSTMENT) && time_diff > 2000) {
      adjustment_substate = FLASH_NAME;
      entered_adjustment = iteration_millis;
    } else if((adjustment_substate == FLASH_NAME) && time_diff > 2000) {
      adjustment_substate = SHOW_VAL;
      entered_adjustment = iteration_millis;
    } else if((adjustment_substate == SHOW_VAL) && time_diff > 499) {
      adjustment_substate = HIDE_VAL;
      entered_adjustment = iteration_millis;
    } else if((adjustment_substate == HIDE_VAL) && time_diff > 499) {
      adjustment_substate = SHOW_VAL;
      entered_adjustment = iteration_millis;
    }
  } else if ((iteration_millis - last_time_sample) > 100) {
    rtc.getTime(timeinfo);
    last_time_sample = iteration_millis;
    if(timeinfo.tm_hour == orange_hour && timeinfo.tm_min == orange_minute) {
      main_state = ON;
      color_state = CUSTOM_SELECTED;
      custom_color_wheel_index = 28; // A very orange color
      brightness = 1;
      update = 1;
      auto_on = 1;
    }
    if(timeinfo.tm_hour == morning_hour && timeinfo.tm_min == morning_minute) {
      main_state = ON;
      color_state = CUSTOM_SELECTED;
      custom_color_wheel_index = 85; // Greeeen
      brightness = 1;
      update = 1;
      auto_on = 1;
    }
    if(timeinfo.tm_hour == (morning_hour + 1) && timeinfo.tm_min == morning_minute && auto_on) {
      main_state = OFF;
      update = 1;
      auto_on = 0;
      custom_color_wheel_index = EEPROM.read(CUSTOM_COLOR_EEPROM);
      color_state = constrain(EEPROM.read(COLOR_STATE_EEPROM),0,3);
    }
  }





  if(btn[1]) {
    btn[1] = 0, update = 1, auto_on = 0; 
    while(bit_is_set(PIND, PD3) == 0 && (millis() - iteration_millis) <= 1500)
    {
    }
    long_press = (millis() - iteration_millis) > 1500;
    if(long_press)
    {
      if(main_state != ADJUSTING){
        adjustment_state = ORANGE_HOUR;
        adjustment_substate = ENTER_ADJUSTMENT;
        main_state = ADJUSTING;
        entered_adjustment = millis();
      } else {
        main_state = OFF;
      }
    }else{
      if(main_state == ADJUSTING) {
        if (adjustment_state == ORANGE_HOUR) {
          EEPROM.write(ORANGE_HOUR_EEPROM, orange_hour);
        } else if (adjustment_state == MORNING_HOUR) {
          EEPROM.write(MORNING_HOUR_EEPROM, morning_hour);
        } else if (adjustment_state == ORANGE_MINUTE) {
          EEPROM.write(ORANGE_MINUTE_EEPROM, orange_minute);
        } else if (adjustment_state == MORNING_MINUTE) {
          EEPROM.write(MORNING_MINUTE_EEPROM, morning_minute);
        } else {
          rtc.setTime(timeinfo);
        }
      }

      main_state = main_state == OFF               ? ON :
                   main_state == ON                ? OFF :
                   adjustment_state == TIME_MINUTE ? OFF :
                                                     ADJUSTING;
      if(adjustment_substate != ENTER_ADJUSTMENT && adjustment_substate != FLASH_NAME) {
        adjustment_substate = FLASH_NAME;
        adjustment_state = static_cast<adjustment_state_t>(static_cast<int>(adjustment_state) + 1);
      }
    }
  }
  if(btn[0]) {
    if(main_state == ON) {
      if(color_state == CUSTOM_SELECTION) {
        EEPROM.write(CUSTOM_COLOR_EEPROM, custom_color_wheel_index);
      }
      color_state = static_cast<color_state_t>((color_state + 1) % NUM_STATES);
      EEPROM.write(COLOR_STATE_EEPROM, static_cast<uint8_t>(color_state));
      update = 1, last_rotation_time = 0;
    } else if(main_state == ADJUSTING) {
      adjustment_diff = adjustment_diff - 1;
    }
    btn[0] = 0, auto_on = 0; 
  }
  if(btn[2]) {
    if(main_state == ON) {
      brightness = brightness == 5 ? 1 : brightness + 1;
      EEPROM.write(BRIGHTNESS_EEPROM, brightness);
      update = 1;
    } else if(main_state == ADJUSTING) {
      adjustment_diff = adjustment_diff + 1;
    }
    
    btn[2] = 0, auto_on = 0;
  }
  rotate_color = ((iteration_millis - last_rotation_time) > 64) && (color_state == CUSTOM_SELECTION);
  custom_color_wheel_index =  rotate_color ? custom_color_wheel_index + 1 : custom_color_wheel_index;
  last_rotation_time = rotate_color ? iteration_millis : last_rotation_time;
  update = update || rotate_color;
  //disp(custom_color_wheel_index, 0);
  if(update) {
    setColor();
    update = 0;
  }

  if(main_state == ADJUSTING) {
    if(adjustment_substate == ENTER_ADJUSTMENT) {
      dispChar("inst", 0);
    }else if(adjustment_substate == FLASH_NAME) {
      switch (adjustment_state) {
        case ORANGE_MINUTE:
        case ORANGE_HOUR: 
          dispChar("oran");
          break;
        case MORNING_MINUTE:
        case MORNING_HOUR:
          dispChar("morn");
          break;
        case YEAR:
          dispChar("year");
          break;
        case MONTH:
          dispChar("mnth");
          break;
        case DAY:
          dispChar("dday", 0, 0b1000);
          break;
        case DOW:
          dispChar("ddow", 0, 0b1000);
          break;
        case TIME_MINUTE:
        case TIME_HOUR:
          dispChar("cclk", 0, 0b1000);
          break;
      }
    }else { // SHOW_VAL and HIDE_VAL
      uint8_t charDisableHide;
      uint8_t charDisableShow;
      uint8_t commaEnable;
      uint16_t value;

      if (adjustment_state == ORANGE_HOUR) {
          charDisableHide = 0b1100;
          charDisableShow = 0;
          commaEnable = 0b0100;
          orange_hour = wrapAround(orange_hour + adjustment_diff, 0,23);
          value = (orange_hour * 100 + orange_minute);
      } else if (adjustment_state == MORNING_HOUR) {
          charDisableHide = 0b1100;
          charDisableShow = 0;
          commaEnable = 0b0100;
          morning_hour = wrapAround(morning_hour + adjustment_diff, 0, 23);
          value = (morning_hour * 100 + morning_minute);
      } else if (adjustment_state == TIME_HOUR) {
          charDisableHide = 0b1100;
          charDisableShow = 0;
          commaEnable = 0b0100;
          timeinfo.tm_hour = wrapAround(timeinfo.tm_hour + adjustment_diff, 0, 23);
          value = (timeinfo.tm_hour * 100 + timeinfo.tm_min);
      } else if (adjustment_state == ORANGE_MINUTE) {
          charDisableHide = 0b0011;
          charDisableShow = 0;
          commaEnable = 0b0100;
          orange_minute = wrapAround(orange_minute + adjustment_diff, 0, 59);
          value = (orange_hour * 100 + orange_minute);
      } else if (adjustment_state == MORNING_MINUTE) {
          charDisableHide = 0b0011;
          charDisableShow = 0;
          commaEnable = 0b0100;
          morning_minute = wrapAround(morning_minute + adjustment_diff, 0, 59);
          value = (morning_hour * 100 + morning_minute);
      } else if (adjustment_state == TIME_MINUTE) {
          charDisableHide = 0b0011;
          charDisableShow = 0;
          commaEnable = 0b0100;
          timeinfo.tm_min = wrapAround(timeinfo.tm_min + adjustment_diff, 0, 59);
          value = (timeinfo.tm_hour * 100 + timeinfo.tm_min);
      } else if (adjustment_state == DAY) {
          charDisableHide = 0b1111;
          charDisableShow = 0b1100;
          commaEnable = 0;
          int16_t daysInMonth = (timeinfo.tm_mon == 1) ? (28 + (timeinfo.tm_year % 4 == 0)) :
                                (timeinfo.tm_mon == 3 || timeinfo.tm_mon == 5 ||
                                 timeinfo.tm_mon == 8 || timeinfo.tm_mon == 10) ? 30 : 31;
          timeinfo.tm_mday = wrapAround(timeinfo.tm_mday + adjustment_diff, 1, daysInMonth);
          value = timeinfo.tm_mday;
      } else if (adjustment_state == MONTH) {
          charDisableHide = 0b1111;
          charDisableShow = 0b1100;
          commaEnable = 0;
          timeinfo.tm_mon = wrapAround(timeinfo.tm_mon + adjustment_diff, 1, 12);
          value = timeinfo.tm_mon;
      } else if (adjustment_state == YEAR) {
          charDisableHide = 0b1111;
          charDisableShow = 0;
          commaEnable = 0;
          timeinfo.tm_year += adjustment_diff;
          value = (timeinfo.tm_year + 1900);
      } else if (adjustment_state == DOW) {
          charDisableHide = 0b1111;
          charDisableShow = 0b1110;
          commaEnable = 0;
          timeinfo.tm_wday = wrapAround(timeinfo.tm_wday + adjustment_diff, 1, 7);
          value = timeinfo.tm_wday;
      } else {
          charDisableHide = 0b1111;
          charDisableShow = 0;
          commaEnable = 0b0100;
          value = 0;
      }

      uint8_t charDisable = (adjustment_substate == SHOW_VAL) ? charDisableShow : charDisableHide;
      disp(value, commaEnable, charDisable);
    }
    adjustment_diff = 0;
  } else {
    disp(0,0,0xf);
  }


}