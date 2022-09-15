#ifndef __DISPLAY_H_
#define __DISPLAY_H_

#include <Adafruit_SharpMem.h>

class Display
{
private:
    Adafruit_SharpMem d;
public:
    Display(uint8_t clk, uint8_t mosi, uint8_t cs) : d(clk, mosi, cs, 96, 96)
    {
    }

    void init()
    {
        d.begin();
        d.clearDisplay();
        d.cp437(true);
        d.setTextSize(1);
        d.setTextWrap(false);

        d.setCursor(1, 1);
        d.setTextColor(0, 1);
        d.print("Packets\n\nPacket rate\n\nData rate");
        d.refresh();
    }

    void setDetailsCount(uint64_t packets, uint64_t packetsPerSec, uint64_t baud)
    {
        d.setCursor(1, 1);
        d.setTextColor(0, 1);
        d.println();
        d.print("  ");
        d.print(packets);
        d.println("   ");
        d.println();
        d.print("  ");
        d.print(packetsPerSec);
        d.println(" /s  ");
        d.println();
        d.print("  ");
        d.print(baud >> 10);
        d.println(" kbps  ");
        d.refresh();
    }

    template <typename T>
    void setStatus(T s)
    {
        d.fillRect(0, 87, 96, 9, 0);

        d.setCursor(0, 88);
        d.setTextColor(1, 0);
        d.print(s);
        d.refresh();
    }
};

#endif // __DISPLAY_H_