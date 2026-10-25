#include "RV3032.h"
#include <Wire.h>

// Register addresses
#define RV3032_REG_100TH_SECONDS   0x00
#define RV3032_REG_SECONDS         0x01
#define RV3032_REG_MINUTES         0x02
#define RV3032_REG_HOURS           0x03
#define RV3032_REG_WEEKDAY         0x04
#define RV3032_REG_DATE            0x05
#define RV3032_REG_MONTH           0x06
#define RV3032_REG_YEAR            0x07
#define RV3032_REG_STATUS          0x0D
#define RV3032_REG_TEMP_LSB        0x0E
#define RV3032_REG_TEMP_MSB        0x0F
#define RV3032_REG_CONTROL1        0x10
#define RV3032_REG_CONTROL2        0x11
#define RV3032_REG_CONTROL3        0x12
#define RV3032_REG_EVI_CONTROL     0x15
#define RV3032_REG_CLK_INT_MASK    0x14
#define RV3032_REG_PMU             0xC0

// Constructor
RV3032::RV3032(uint8_t i2cAddress, TwoWire &wirePort) 
    : _i2cAddress(i2cAddress), _wire(wirePort) {
}

// Initialize the RTC
bool RV3032::begin() {
    _wire.begin();
    
    // Check if device is connected
    if (!isConnected()) {
        return false;
    }
    
    // Disable EVI functionality
    if (!disableEVI()) {
        return false;
    }

    if(!writeModify(RV3032_REG_PMU, 0b00100000, 0b11001111)) { // Set the device to switch to battery backup 
        return false;
    }
    // Setup 1Hz interrupt
    // if (!setup1HzInterrupt()) {
    //     return false;
    // }
    
    // // Enable interrupt output
    // if (!enableInterruptOutput()) {
    //     return false;
    // }
    
    // Clear any pending interrupt flags
    writeByte(RV3032_REG_STATUS, 0x00);
    
    return true;
}

// Check if RTC is connected
bool RV3032::isConnected() {
    _wire.beginTransmission(_i2cAddress);
    return (_wire.endTransmission() == 0);
}

// Disable EVI functionality
bool RV3032::disableEVI() {
    // Set EVI Control register to 0x00 to disable EVI
    return writeByte(RV3032_REG_EVI_CONTROL, 0x00);
}

bool RV3032::writeModify(uint8_t address, uint8_t data, uint8_t mask) {
    uint8_t regvalue = readByte(address);
    regvalue &= mask;
    regvalue |= data;
    return writeByte(address, regvalue);
}

// Setup 1Hz interrupt
bool RV3032::setup1HzInterrupt() {
    // Control 1 register: USEL=0 (second update), TE=0 (timer disabled)
    // This sets periodic time update to seconds
    return writeByte(RV3032_REG_CONTROL1, 0x00);
}

// Enable interrupt output
bool RV3032::enableInterruptOutput() {
    // Control 2 register: UIE=1 (enable update interrupt)
    uint8_t control2 = readByte(RV3032_REG_CONTROL2);
    control2 |= (1 << 5);  // Set UIE bit (bit 5)
    control2 &= ~(1 << 0); // Ensure STOP bit is 0 (clock running)
    
    return writeByte(RV3032_REG_CONTROL2, control2);
}

// Set time from tm structure
bool RV3032::setTime(const tm &time) {
    return writeRTCTime(time);
}

// Get time to tm structure
bool RV3032::getTime(tm &time) {
    time = readRTCTime();
    return true;
}

// Get Norwegian local time (CEST in summer, CET in winter)
bool RV3032::getLocalTime(tm &time) {
    time = readRTCTime();
    time.tm_isdst = isDSTNorway(time);
    if (time.tm_isdst) {
        shiftHour(time, 1);
    }
    return true;
}

// Set the RTC from Norwegian local time, storing it as standard time
bool RV3032::setLocalTime(const tm &time) {
    // In summer the local time is one hour ahead of standard time. Use that if
    // the resulting standard time is in the DST period, otherwise the local
    // time already is standard time. In the repeated hour in October
    // (02:00-02:59) this picks the first, summer time, occurrence.
    tm standardTime = time;
    shiftHour(standardTime, -1);
    if (!isDSTNorway(standardTime)) {
        standardTime = time;
    }
    return writeRTCTime(standardTime);
}

// Check if it's DST in Norway at the given standard time (CET). DST runs from
// the last Sunday in March to the last Sunday in October and switches at
// 01:00 UTC, which is 02:00 CET at both ends.
bool RV3032::isDSTNorway(const tm &standardTime) {
    int year = standardTime.tm_year + 1900;
    uint8_t month = standardTime.tm_mon + 1;
    
    if (month < 3 || month > 10) {
        return false;
    }
    if (month > 3 && month < 10) {
        return true;
    }
    
    // March and October both have 31 days
    uint8_t lastSunday = 31 - dayOfWeek(year, month, 31);
    bool afterSwitch = standardTime.tm_mday > lastSunday ||
                       (standardTime.tm_mday == lastSunday && standardTime.tm_hour >= 2);
    return (month == 3) ? afterSwitch : !afterSwitch;
}

// Days in month (0-11) of the given year
uint8_t RV3032::daysInMonth(int year, uint8_t month) {
    if (month == 1) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return leap ? 29 : 28;
    }
    return (month == 3 || month == 5 || month == 8 || month == 10) ? 30 : 31;
}

// Day of week for a date (month 1-12), 0 = Sunday (Sakamoto's method)
uint8_t RV3032::dayOfWeek(int year, uint8_t month, uint8_t day) {
    static const uint8_t offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) {
        year -= 1;
    }
    return (year + year / 4 - year / 100 + year / 400 + offset[month - 1] + day) % 7;
}

// Move the time one hour forward (dir = 1) or back (dir = -1), rolling the date
void RV3032::shiftHour(tm &time, int8_t dir) {
    time.tm_hour += dir;
    if (time.tm_hour > 23) {
        time.tm_hour = 0;
        time.tm_wday = (time.tm_wday + 1) % 7;
        if (++time.tm_mday > daysInMonth(time.tm_year + 1900, time.tm_mon)) {
            time.tm_mday = 1;
            if (++time.tm_mon > 11) {
                time.tm_mon = 0;
                time.tm_year++;
            }
        }
    } else if (time.tm_hour < 0) {
        time.tm_hour = 23;
        time.tm_wday = (time.tm_wday + 6) % 7;
        if (--time.tm_mday < 1) {
            if (--time.tm_mon < 0) {
                time.tm_mon = 11;
                time.tm_year--;
            }
            time.tm_mday = daysInMonth(time.tm_year + 1900, time.tm_mon);
        }
    }
}

// Read temperature in Celsius
float RV3032::readTemperature() {
    uint8_t tempLSB = readByte(RV3032_REG_TEMP_LSB);
    uint8_t tempMSB = readByte(RV3032_REG_TEMP_MSB);
    
    // Temperature is 12-bit two's complement
    int16_t rawTemp = (tempMSB << 4) | ((tempLSB >> 4) & 0x0F);
    
    // Handle negative temperatures (two's complement)
    if (rawTemp & 0x0800) {
        rawTemp |= 0xF000;
    }
    
    return rawTemp * 0.0625f;
}

// Get current RTC status
uint8_t RV3032::getStatus() {
    return readByte(RV3032_REG_STATUS);
}

// Read RTC time and convert to tm structure
tm RV3032::readRTCTime() {
    tm time = {0};
    uint8_t data[7];
    
    readBytes(RV3032_REG_SECONDS, data, 7);
    
    time.tm_sec = bcdToDec(data[0] & 0x7F);
    time.tm_min = bcdToDec(data[1] & 0x7F);
    time.tm_hour = bcdToDec(data[2] & 0x3F);
    time.tm_wday = data[3] & 0x07;
    time.tm_mday = bcdToDec(data[4] & 0x3F);
    time.tm_mon = bcdToDec(data[5] & 0x1F) - 1;
    
    // Year: RTC stores 00-99 for years 2000-2099
    uint8_t year_bcd = data[6];
    uint8_t year_dec = bcdToDec(year_bcd);
    
    // Convert RTC's 00-99 to tm_year (years since 1900)
    if (year_dec <= 99) {
        time.tm_year = year_dec + 100;  // 0-99 -> 100-199 (2000-2099)
    } else {
        time.tm_year = 100;
    }
    
    return time;
}

// Write tm structure to RTC
bool RV3032::writeRTCTime(const tm &time) {
    uint8_t data[7];
    
    data[0] = decToBcd(time.tm_sec);
    data[1] = decToBcd(time.tm_min);
    data[2] = decToBcd(time.tm_hour);
    data[3] = time.tm_wday & 0x07;
    data[4] = decToBcd(time.tm_mday);
    data[5] = decToBcd(time.tm_mon + 1);
    
    // Convert tm_year (years since 1900) to RTC's 00-99 format
    int year_for_rtc;
    
    if (time.tm_year >= 100) {
        year_for_rtc = time.tm_year - 100;
    } else if (time.tm_year >= 0) {
        year_for_rtc = time.tm_year;
    } else {
        year_for_rtc = 0;
    }
    
    year_for_rtc = year_for_rtc % 100;
    data[6] = decToBcd(year_for_rtc);
    
    return writeBytes(RV3032_REG_SECONDS, data, 7);
}

// Helper: BCD to decimal
uint8_t RV3032::bcdToDec(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// Helper: Decimal to BCD
uint8_t RV3032::decToBcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

// I2C write single byte
bool RV3032::writeByte(uint8_t reg, uint8_t data) {
    _wire.beginTransmission(_i2cAddress);
    _wire.write(reg);
    _wire.write(data);
    return (_wire.endTransmission() == 0);
}

// I2C write multiple bytes
bool RV3032::writeBytes(uint8_t reg, uint8_t *data, uint8_t length) {
    _wire.beginTransmission(_i2cAddress);
    _wire.write(reg);
    for (uint8_t i = 0; i < length; i++) {
        _wire.write(data[i]);
    }
    return (_wire.endTransmission() == 0);
}

// I2C read single byte
uint8_t RV3032::readByte(uint8_t reg) {
    _wire.beginTransmission(_i2cAddress);
    _wire.write(reg);
    _wire.endTransmission(false);
    
    _wire.requestFrom(_i2cAddress, (uint8_t)1);
    if (_wire.available()) {
        return _wire.read();
    }
    return 0;
}

// I2C read multiple bytes
bool RV3032::readBytes(uint8_t reg, uint8_t *data, uint8_t length) {
    _wire.beginTransmission(_i2cAddress);
    _wire.write(reg);
    _wire.endTransmission(false);
    
    uint8_t bytesRead = _wire.requestFrom(_i2cAddress, length);
    if (bytesRead == length) {
        for (uint8_t i = 0; i < length; i++) {
            data[i] = _wire.read();
        }
        return true;
    }
    return false;
}