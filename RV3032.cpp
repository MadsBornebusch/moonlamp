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

// Adjust time for Norwegian daylight saving
bool RV3032::adjustNorwegianDST(tm &time) {
    // Check if we need to adjust for DST
    bool isDST = isDSTNorway(time);
    
    // Read current time from RTC
    tm currentTime = readRTCTime();
    
    // Check if current time matches the expected DST state
    bool currentIsDST = isDSTNorway(currentTime);
    
    // If DST state has changed, adjust the time
    if (isDST != currentIsDST) {
        if (isDST) {
            // Spring forward: add 1 hour
            currentTime.tm_hour += 1;
            if (currentTime.tm_hour >= 24) {
                currentTime.tm_hour -= 24;
                currentTime.tm_mday += 1;
            }
        } else {
            // Fall back: subtract 1 hour
            if (currentTime.tm_hour == 0) {
                currentTime.tm_hour = 23;
                currentTime.tm_mday -= 1;
            } else {
                currentTime.tm_hour -= 1;
            }
        }
        
        // Write adjusted time back to RTC
        return writeRTCTime(currentTime);
    }
    
    return true;
}

// Check if it's DST in Norway at the given time
bool RV3032::isDSTNorway(const tm &time) {
    int year = time.tm_year + 1900;
    int month = time.tm_mon + 1;
    int day = time.tm_mday;
    int hour = time.tm_hour;
    
    // Find last Sunday in March
    tm lastMar = {0};
    lastMar.tm_year = year - 1900;
    lastMar.tm_mon = 2;
    lastMar.tm_mday = 31;
    mktime(&lastMar);
    int lastSundayMar = lastMar.tm_mday - lastMar.tm_wday;
    
    // Find last Sunday in October
    tm lastOct = {0};
    lastOct.tm_year = year - 1900;
    lastOct.tm_mon = 9;
    lastOct.tm_mday = 31;
    mktime(&lastOct);
    int lastSundayOct = lastOct.tm_mday - lastOct.tm_wday;
    
    // Check if current date is in DST period
    if (month > 3 && month < 10) {
        return true;
    } else if (month == 3) {
        if (day > lastSundayMar) {
            return true;
        } else if (day == lastSundayMar && hour >= 1) {
            return true;
        }
    } else if (month == 10) {
        if (day < lastSundayOct) {
            return true;
        } else if (day == lastSundayOct && hour < 1) {
            return true;
        }
    }
    
    return false;
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