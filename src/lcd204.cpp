#include <Arduino.h>
#include "lcd204.h"

#include <Wire.h>
#include <hd44780.h>                       // main hd44780 header
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c expander i/o class header

hd44780_I2Cexp lcd; // declare lcd object: auto locate & auto config expander chip

const int LCD_COLS = 20;
const int LCD_ROWS = 4;

void lcd2004Setup()
{
    int status;

    //Wire.setClock(400000);      // Set the I2C SCL to 400kHz
    Wire.begin(I2C_SDA, I2C_SCL);

    lcd.setExecTimes(2000, 600);
    status = lcd.begin(LCD_COLS, LCD_ROWS);
    if (status) { // non zero status means it was unsuccesful
        Serial.println("LCD 2004A Init Failed !!!");
        hd44780::fatalError(status); // never return ...
    }
    lcd.setCursor(0,0);
    // Print a message to the LCD
    lcd.printf("  *** F3F Timer ***  ");	
}

void lcd2004Loop()
{
	static unsigned long lastUpdate = 0;

	if (millis() - lastUpdate >= 1000) {
		lastUpdate = millis();

		//lcd.clear();
/*
		lcd.setCursor(0,0);
		lcd.printf("%10ld%10ld", r0, r1);

		lcd.setCursor(0,2);
		lcd.printf("%7.2fg %7.2fg", w0, w1);

		lcd.setCursor(0,3);
		lcd.printf("CG %5.2f mm", cg);
*/    
	}
}

SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

void lcdPrintRow(uint8_t row, const char *fmt, ...)
{
    xSemaphoreTake(mutex, portMAX_DELAY); // enter critical section

    char str[LINE_SIZE+1];
    va_list args;
  
    va_start(args, fmt);
    int r = vsnprintf(str, LINE_SIZE+1, fmt, args);
    va_end(args);

    Serial.printf("%s\t(%d)\r\n", str, r);

    if(r >= 0) {
        if(r < LINE_SIZE)
            memset(str+r, 0x20, LINE_SIZE-r);
        str[LINE_SIZE] = '\0';
        lcd.setCursor(0, row);
        lcd.printf(str);            
    }

    xSemaphoreGive(mutex); // exit critical section
}

// Must manually destroy the mutex when no longer needed:
//vSemaphoreDelete(mutex);

void lcdNoCursor()
{
    lcd.noCursor();
}

void lcdClear()
{
    lcd.clear();
}

