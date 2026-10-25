#ifndef RV3032_H
#define RV3032_H

#include <Arduino.h>
#include <Wire.h>
#include <time.h>

class RV3032 {
public:
    // Constructor
    RV3032(uint8_t i2cAddress = 0x51, TwoWire &wirePort = Wire);
    
    // Initialize the RTC
    bool begin();
    
    // Check if RTC is connected
    bool isConnected();
    
    // Set time from tm structure
    bool setTime(const tm &time);
    
    // Get time to tm structure
    bool getTime(tm &time);

    // Get/set Norwegian local time. The RTC always runs on standard time (CET),
    // summer time (CEST) is added when reading and removed when writing.
    bool getLocalTime(tm &time);
    bool setLocalTime(const tm &time);

    // Read temperature in Celsius
    float readTemperature();

    // Check if it's DST in Norway at the given standard time (CET)
    static bool isDSTNorway(const tm &standardTime);

    // Days in month (0-11) of the given year
    static uint8_t daysInMonth(int year, uint8_t month);
    
    // Get current RTC status
    uint8_t getStatus();

private:
    uint8_t _i2cAddress;
    TwoWire &_wire;
    
    // I2C communication
    bool writeByte(uint8_t reg, uint8_t data);
    bool writeBytes(uint8_t reg, uint8_t *data, uint8_t length);
    uint8_t readByte(uint8_t reg);
    bool readBytes(uint8_t reg, uint8_t *data, uint8_t length);
    
    // Helper functions
    uint8_t bcdToDec(uint8_t bcd);
    uint8_t decToBcd(uint8_t dec);
    static uint8_t dayOfWeek(int year, uint8_t month, uint8_t day);
    static void shiftHour(tm &time, int8_t dir);

    // Configuration functions
    bool disableEVI();
    bool writeModify(uint8_t address, uint8_t data, uint8_t mask);
    bool setup1HzInterrupt();
    bool enableInterruptOutput();
    
    // Time conversion
    tm readRTCTime();
    bool writeRTCTime(const tm &time);
};

#endif // RV3032_H