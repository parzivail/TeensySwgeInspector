#include <Arduino.h>

/*
	Test code to drive a display from a Palm PDA (grayscale display, not color)
	Notes:
	 - The pixel clock is nominally 1.3 MHz on a PDA but has been tested up to around 100 MHz
	 - The line clock nominally is 13 kHz but has been tested up to around 125 kHz
	 - The frame clock nominally is around 90 Hz but has been tested up to around 700 Hz

	Pinout:
	
	 1 - EL1
	 2 - EL2
	 3 - Test Point
	 4 - FLM
	 5 - LP
	 6 - CP
	 7 - LCD_DISP_ON
	 8 - LCD_VCC_ON
	 9 - GND
	10 - LCD_BIAS
	11 - LD0
	12 - LD1
	13 - LD2
	14 - LD3
	15 - GND
	16 - GND
	17 - TP_X+
	18 - TP_X-
	19 - TP_Y+
	20 - TP_Y-
*/

#define LD3 (37)
#define LD2 (38)
#define LD1 (39)
#define LD0 (40)

#define LCD_DISP_ON (13)
#define CP (14)
#define LP (15)
#define FLM (16)

static uint8_t display_buffer[3200] = {0};

// Line clock: 13.2 kHz
// Pixel clock: 1.3 MHz

void setupPalm()
{
	pinMode(LD0, OUTPUT);
	pinMode(LD1, OUTPUT);
	pinMode(LD2, OUTPUT);
	pinMode(LD3, OUTPUT);
	pinMode(LCD_DISP_ON, OUTPUT);
	pinMode(CP, OUTPUT);
	pinMode(LP, OUTPUT);
	pinMode(FLM, OUTPUT);

	for (uint16_t i = 0; i < 160; i++)
	{
		for (uint16_t j = 0; j < 20; j++)
			display_buffer[i * 20 + j] = (i % 2 == 0) ? 0b10101010 : 0b01010101;
	}

	digitalWriteFast(LCD_DISP_ON, HIGH);
}

void outputPixelNibble(uint8_t nibble)
{
	digitalWriteFast(CP, HIGH);
	digitalWriteFast(LD0, (nibble & 0b0001) != 0);
	digitalWriteFast(LD1, (nibble & 0b0010) != 0);
	digitalWriteFast(LD2, (nibble & 0b0100) != 0);
	digitalWriteFast(LD3, (nibble & 0b1000) != 0);
	// delayNanoseconds(362);
	delayNanoseconds(10);

	digitalWriteFast(CP, LOW);
	delayNanoseconds(10);
}

void outputRow(uint8_t rowIdx)
{
	for (uint8_t pixel = 0; pixel < 20; pixel++)
	{
		outputPixelNibble((display_buffer[rowIdx * 20 + pixel] & 0xF0) >> 4);
		outputPixelNibble(display_buffer[rowIdx * 20 + pixel] & 0x0F);
	}

	digitalWriteFast(LP, HIGH);
	delayNanoseconds(5);
	digitalWriteFast(LP, LOW);
}

// #define ROW_WAIT (8000)
// #define ROW_WAIT (10000)
#define ROW_WAIT (20000)
// #define ROW_WAIT (40000)
void outputFrame()
{
	outputRow(0);
	digitalWriteFast(FLM, HIGH);
	delayNanoseconds(ROW_WAIT);
	outputRow(1);
	digitalWriteFast(FLM, LOW);
	delayNanoseconds(ROW_WAIT);

	for (uint8_t row = 2; row < 160; row++)
	{
		outputRow(row);
		delayNanoseconds(ROW_WAIT);
	}
}

void loopPalm()
{
	outputFrame();
}