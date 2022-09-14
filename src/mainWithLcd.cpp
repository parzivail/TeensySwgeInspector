#include "tcp802.h"

Tcp802 lcd(17, 18, 19);

void setupWithLcd()
{
    lcd.init();

    lcd.setSegment(LCD_DECIMAL_3, true);
    lcd.commitSegments();
}

void loopWithLcd()
{
    uint32_t m = millis() / 100;
    lcd.displayInteger(m);
    lcd.commitSegments();
}