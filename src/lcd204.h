#ifndef LCD204_H
#define LCD204_H

#include <stdint.h>
// I2C
#define I2C_SDA       15
#define I2C_SCL       0

#define LINE_SIZE 20

void lcd2004Setup();
void lcd2004Loop();

void lcdPrintRow(uint8_t row, const char *fmt, ...);
void lcdNoCursor();
void lcdClear();

#endif