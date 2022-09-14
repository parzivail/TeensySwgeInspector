#include <Arduino.h>

#ifndef __TCP802_H_
#define __TCP802_H_

// These modules are found in various CyberPower UPS systems. VDD, LED, and backlight are at least 5v tolerant.
// Module: https://www.globalsources.com/Alphanumeric-LCD/Alphanumeric-LCD-Module-1172373840p.htm
// Controller IC datasheet: https://pan.baidu.com/link/zhihu/7dhjzRuShLiGV2IUFmUrNfZUSDaOVkcwZn92==

/*
	Pinout:

	 1 - SW2-1
	 2 - SW2-2
	 3 - VDD
	 4 - GND
	 5 - DIO
	 6 - WRB
	 7 - RDB
	 8 - LED+
	 9 - Backlight+
	10 - SW3+ (SW1+ on 10-pin variants)
	11 - SW4+ (omitted on 10-pin variants)
*/

#define TCP802_PRE_CODE (0b1)
#define TCP802_MODE_COMMAND (0b00)
#define TCP802_MODE_WRITE (0b01)

// Data clock cap is 150 kHz, so we use a 4 (*2) us delay to achieve 125kHz cap
// #define CLOCK_HALF_DELAY (4000)
// Tested to work as low as 0.05us per half clock (10 MHz cap)
#define CLOCK_HALF_DELAY (50)

/*
	``_a_
	f|   |b
	`|_g_|
	e|   |c
	`|_d_|
*/

// Based on
// * https://github.com/adafruit/Adafruit_LED_Backpack/blob/master/Adafruit_LEDBackpack.cpp
// * https://fakoo.de/en/siekoo.html
static const PROGMEM uint8_t SEGMENT_FONT_TABLE[] = {

	0b00000000, // (space)
	0b01101011, // !
	0b00100010, // "
	0b00110110, // #
	0b00001000, // $
	0b00100100, // %
	0b00100000, // &
	0b00100000, // '
	0b00111001, // (
	0b00001111, // )
	0b01001001, // *
	0b01000110, // +
	0b00001100, // ,
	0b01000000, // -
	0b00010000, // .
	0b01010010, // /
	0b00111111, // 0
	0b00000110, // 1
	0b01011011, // 2
	0b01001111, // 3
	0b01100110, // 4
	0b01101101, // 5
	0b01111101, // 6
	0b00000111, // 7
	0b01111111, // 8
	0b01101111, // 9
	0b00001001, // :
	0b00001010, // ;
	0b00100001, // <
	0b01001000, // =
	0b00000011, // >
	0b01001011, // ?
	0b00010111, // @
	0b01011111, // A
	0b01111100, // B
	0b01011000, // C
	0b01011110, // D
	0b01111001, // E
	0b01110001, // F
	0b00111101, // G
	0b01110100, // H
	0b00010001, // I
	0b00001101, // J
	0b01110101, // K
	0b00111000, // L
	0b01010101, // M
	0b01010100, // N
	0b01011100, // O
	0b01110011, // P
	0b01100111, // Q
	0b01010000, // R
	0b00101101, // S
	0b01111000, // T
	0b00011100, // U
	0b00101010, // V
	0b01101010, // W
	0b00010100, // X
	0b01101110, // Y
	0b00011011, // Z
	0b00111001, // [
	0b01100100, // Backslash
	0b00001111, // ]
	0b00100011, // ^
	0b00001000, // _
	0b00000010, // `
	0b01011111, // a
	0b01111100, // b
	0b01011000, // c
	0b01011110, // d
	0b01111001, // e
	0b01110001, // f
	0b00111101, // g
	0b01110100, // h
	0b00010001, // i
	0b00001101, // j
	0b01110101, // k
	0b00111000, // l
	0b01010101, // m
	0b01010100, // n
	0b01011100, // o
	0b01110011, // p
	0b01100111, // q
	0b01010000, // r
	0b00101101, // s
	0b01111000, // t
	0b00011100, // u
	0b00101010, // v
	0b01101010, // w
	0b00010100, // x
	0b01101110, // y
	0b00011011, // z
	0b01000110, // {
	0b00110000, // |
	0b01110000, // }
	0b00000001, // ~
	0b00000000, // del
};

enum : uint8_t
{
	LCD_BATTERY_CAPACITY = (0x00 << 2) | 0b10,
	LCD_BATTERY_BAR_ICON = (0x1E << 2) | 0b01,
	LCD_BATTERY_BAR_1 = (0x00 << 2) | 0b01,
	LCD_BATTERY_BAR_2 = (0x16 << 2) | 0b01,
	LCD_BATTERY_BAR_3 = (0x06 << 2) | 0b01,
	LCD_BATTERY_BAR_4 = (0x1A << 2) | 0b01,
	LCD_BATTERY_BAR_5 = (0x0A << 2) | 0b01,
	LCD_LOAD_CAPACITY = (0x0E << 2) | 0b01,
	LCD_LOAD_BAR_ICON = (0x1E << 2) | 0b10,
	LCD_LOAD_BAR_1 = (0x0E << 2) | 0b10,
	LCD_LOAD_BAR_2 = (0x16 << 2) | 0b10,
	LCD_LOAD_BAR_3 = (0x06 << 2) | 0b10,
	LCD_LOAD_BAR_4 = (0x1A << 2) | 0b10,
	LCD_LOAD_BAR_5 = (0x0A << 2) | 0b10,
	LCD_NORMAL_ICON = (0x10 << 2) | 0b01,
	LCD_BATTERY_ICON = (0x08 << 2) | 0b01,
	LCD_AVR_ICON = (0x18 << 2) | 0b01,
	LCD_SILENT_ICON = (0x12 << 2) | 0b01,
	LCD_OVER_LOAD_ICON = (0x02 << 2) | 0b01,
	LCD_FAULT_ICON = (0x1C << 2) | 0b01,
	LCD_INPUT_BOX = (0x10 << 2) | 0b10,
	LCD_OUTPUT_BOX = (0x08 << 2) | 0b10,
	LCD_ESTIMATED_RUNTIME_BOX = (0x18 << 2) | 0b10,
	LCD_SEG_1A = (0x15 << 2) | 0b10,
	LCD_SEG_1B = (0x0D << 2) | 0b10,
	LCD_SEG_1C = (0x0D << 2) | 0b01,
	LCD_SEG_1D = (0x19 << 2) | 0b01,
	LCD_SEG_1E = (0x05 << 2) | 0b01,
	LCD_SEG_1F = (0x05 << 2) | 0b10,
	LCD_SEG_1G = (0x15 << 2) | 0b01,
	LCD_DECIMAL_1 = (0x19 << 2) | 0b10,
	LCD_SEG_2A = (0x03 << 2) | 0b10,
	LCD_SEG_2B = (0x13 << 2) | 0b10,
	LCD_SEG_2C = (0x13 << 2) | 0b01,
	LCD_SEG_2D = (0x09 << 2) | 0b01,
	LCD_SEG_2E = (0x1D << 2) | 0b01,
	LCD_SEG_2F = (0x1D << 2) | 0b10,
	LCD_SEG_2G = (0x03 << 2) | 0b01,
	LCD_DECIMAL_2 = (0x09 << 2) | 0b10,
	LCD_COLON = (0x11 << 2) | 0b10,
	LCD_SEG_3A = (0x1B << 2) | 0b10,
	LCD_SEG_3B = (0x07 << 2) | 0b10,
	LCD_SEG_3C = (0x07 << 2) | 0b01,
	LCD_SEG_3D = (0x11 << 2) | 0b01,
	LCD_SEG_3E = (0x0B << 2) | 0b01,
	LCD_SEG_3F = (0x0B << 2) | 0b10,
	LCD_SEG_3G = (0x1B << 2) | 0b01,
	LCD_DECIMAL_3 = (0x01 << 2) | 0b10,
	LCD_SEG_4A = (0x0F << 2) | 0b10,
	LCD_SEG_4B = (0x1F << 2) | 0b10,
	LCD_SEG_4C = (0x1F << 2) | 0b01,
	LCD_SEG_4D = (0x01 << 2) | 0b01,
	LCD_SEG_4E = (0x17 << 2) | 0b01,
	LCD_SEG_4F = (0x17 << 2) | 0b10,
	LCD_SEG_4G = (0x0F << 2) | 0b01,
	LCD_V = (0x12 << 2) | 0b10,
	LCD_A = (0x04 << 2) | 0b10,
	LCD_HZ = (0x04 << 2) | 0b01,
	LCD_KW = (0x02 << 2) | 0b10,
	LCD_VA = (0x14 << 2) | 0b10,
	LCD_MIN = (0x14 << 2) | 0b01,
	LCD_PERCENT = (0x1C << 2) | 0b10,
	LCD_DEG_C = (0x0C << 2) | 0b01,
	LCD_DEG_F = (0x0C << 2) | 0b10,
};

uint8_t SEGMENT_MAP[4][7] = {
	{LCD_SEG_1A, LCD_SEG_1B, LCD_SEG_1C, LCD_SEG_1D, LCD_SEG_1E, LCD_SEG_1F, LCD_SEG_1G},
	{LCD_SEG_2A, LCD_SEG_2B, LCD_SEG_2C, LCD_SEG_2D, LCD_SEG_2E, LCD_SEG_2F, LCD_SEG_2G},
	{LCD_SEG_3A, LCD_SEG_3B, LCD_SEG_3C, LCD_SEG_3D, LCD_SEG_3E, LCD_SEG_3F, LCD_SEG_3G},
	{LCD_SEG_4A, LCD_SEG_4B, LCD_SEG_4C, LCD_SEG_4D, LCD_SEG_4E, LCD_SEG_4F, LCD_SEG_4G},
};

uint8_t DECIMAL_MAP[] = {LCD_DECIMAL_1, LCD_DECIMAL_2, LCD_DECIMAL_3};

class Tcp802
{
private:
	int pinDio;
	int pinWrb;
	int pinRdb;

	uint8_t REGISTERS[0x20] = {0};
	bool COMMIT_BUFFER[0x20] = {0};

	void writeBit(uint8_t bit)
	{
		digitalWriteFast(pinWrb, LOW);
		digitalWriteFast(pinDio, bit != 0);
		delayNanoseconds(CLOCK_HALF_DELAY);
		digitalWriteFast(pinWrb, HIGH);
		delayNanoseconds(CLOCK_HALF_DELAY);
	}

	void writeCommand(uint8_t preCode, uint8_t modeCode, uint8_t command)
	{
		digitalWriteFast(pinWrb, LOW);
		digitalWriteFast(pinRdb, LOW);
		delayNanoseconds(CLOCK_HALF_DELAY);
		digitalWriteFast(pinRdb, HIGH);
		delayNanoseconds(CLOCK_HALF_DELAY);

		writeBit(preCode & 0b1);
		writeBit(modeCode & 0b10);
		writeBit(modeCode & 0b01);
		writeBit(command & 0b10000000);
		writeBit(command & 0b01000000);
		writeBit(command & 0b00100000);
		writeBit(command & 0b00010000);
		writeBit(command & 0b00001000);
		writeBit(command & 0b00000100);
		writeBit(command & 0b00000010);
		writeBit(command & 0b00000001);
		writeBit(0);

		digitalWriteFast(pinWrb, HIGH);
		digitalWriteFast(pinRdb, LOW);
		delayNanoseconds(CLOCK_HALF_DELAY);
		digitalWriteFast(pinRdb, HIGH);
		delayNanoseconds(CLOCK_HALF_DELAY);
	}

	void writeData(uint8_t preCode, uint8_t modeCode, uint8_t address, uint8_t data)
	{
		digitalWriteFast(pinWrb, LOW);
		digitalWriteFast(pinRdb, LOW);
		delayNanoseconds(CLOCK_HALF_DELAY);
		digitalWriteFast(pinRdb, HIGH);
		delayNanoseconds(CLOCK_HALF_DELAY);

		writeBit(preCode & 0b1);
		writeBit(modeCode & 0b10);
		writeBit(modeCode & 0b01);
		writeBit(0);
		writeBit(address & 0b00001);
		writeBit(address & 0b00010);
		writeBit(address & 0b00100);
		writeBit(address & 0b01000);
		writeBit(address & 0b10000);
		writeBit(data & 0b0001);
		writeBit(data & 0b0010);
		writeBit(data & 0b0100);
		writeBit(data & 0b1000);

		digitalWriteFast(pinWrb, HIGH);
		digitalWriteFast(pinRdb, LOW);
		delayNanoseconds(CLOCK_HALF_DELAY);
		digitalWriteFast(pinRdb, HIGH);
		delayNanoseconds(CLOCK_HALF_DELAY);
	}

public:
	Tcp802(int dio, int wrb, int rdb) : pinDio(dio), pinWrb(wrb), pinRdb(rdb)
	{
		pinMode(dio, OUTPUT);
		pinMode(wrb, OUTPUT);
		pinMode(rdb, OUTPUT);
	}

	void commitSegments()
	{
		for (uint8_t i = 0; i < 0x20; i++)
			if (COMMIT_BUFFER[i])
				writeData(TCP802_PRE_CODE, TCP802_MODE_WRITE, i, REGISTERS[i]);
	}

	void setSegment(uint8_t address, uint8_t mask, bool value)
	{
		address &= 0x1F;
		mask &= 0b11;

		if (value)
			REGISTERS[address] |= mask;
		else
			REGISTERS[address] &= ~mask;

		COMMIT_BUFFER[address] = true;
	}

	bool getSegment(uint8_t address, uint8_t mask)
	{
		address &= 0x1F;
		mask &= 0b11;

		return (REGISTERS[address] & mask) != 0;
	}

	void setSegment(uint8_t segment, bool value)
	{
		setSegment((segment & 0b11111100) >> 2, segment & 0b11, value);
	}

	bool getSegment(uint8_t segment)
	{
		return getSegment((segment & 0b11111100) >> 2, segment & 0b11);
	}

	void displayDigitRaw(uint8_t digit, uint8_t segments)
	{
		setSegment(SEGMENT_MAP[digit][0], segments & 0b0000001);
		setSegment(SEGMENT_MAP[digit][1], segments & 0b0000010);
		setSegment(SEGMENT_MAP[digit][2], segments & 0b0000100);
		setSegment(SEGMENT_MAP[digit][3], segments & 0b0001000);
		setSegment(SEGMENT_MAP[digit][4], segments & 0b0010000);
		setSegment(SEGMENT_MAP[digit][5], segments & 0b0100000);
		setSegment(SEGMENT_MAP[digit][6], segments & 0b1000000);
	}

	void displayCharacter(uint8_t digit, uint8_t character)
	{
		displayDigitRaw(digit, pgm_read_byte(SEGMENT_FONT_TABLE + character - 32));
	}

	void displayInteger(uint16_t value)
	{
		uint8_t chars[4] = {' ', ' ', ' ', ' '};

		if (value == 0)
			chars[3] = '0';
		else
		{
			chars[3] = '0' + (value % 10);
			if (value > 9)
				chars[2] = '0' + ((value / 10) % 10);
			if (value > 99)
				chars[1] = '0' + ((value / 100) % 10);
			if (value > 999)
				chars[0] = '0' + ((value / 1000) % 10);
		}

		displayCharacter(0, chars[0]);
		displayCharacter(1, chars[1]);
		displayCharacter(2, chars[2]);
		displayCharacter(3, chars[3]);
	}

	void displayFloat(float_t value)
	{
		uint8_t decimal = 3;

		if (value < 10)
		{
			value *= 1000;
			decimal = 0;
		}
		else if (value < 100)
		{
			value *= 100;
			decimal = 1;
		}
		else if (value < 1000)
		{
			value *= 10;
			decimal = 2;
		}

		displayInteger((uint16_t)value);
		if (decimal < 3)
			setSegment(DECIMAL_MAP[decimal], true);
	}

	void displaySpinner(uint8_t digit, uint8_t frame)
	{
		displayDigitRaw(digit, 1 << (frame % 6));
	}

	void displayTime(uint8_t major, uint8_t minor)
	{
		uint8_t chars[4] = {' '};

		if (major > 9)
			chars[0] = '0' + ((major / 10) % 10);
		chars[1] = '0' + (major % 10);

		chars[2] = '0' + ((minor / 10) % 10);
		chars[3] = '0' + (minor % 10);

		displayCharacter(0, chars[0]);
		displayCharacter(1, chars[1]);
		displayCharacter(2, chars[2]);
		displayCharacter(3, chars[3]);
	}

	void displayLoadBar(uint8_t value)
	{
		setSegment(LCD_LOAD_BAR_1, value > 0);
		setSegment(LCD_LOAD_BAR_2, value > 1);
		setSegment(LCD_LOAD_BAR_3, value > 2);
		setSegment(LCD_LOAD_BAR_4, value > 3);
		setSegment(LCD_LOAD_BAR_5, value > 4);
	}

	void displayBatteryBar(uint8_t value)
	{
		setSegment(LCD_BATTERY_BAR_1, value > 0);
		setSegment(LCD_BATTERY_BAR_2, value > 1);
		setSegment(LCD_BATTERY_BAR_3, value > 2);
		setSegment(LCD_BATTERY_BAR_4, value > 3);
		setSegment(LCD_BATTERY_BAR_5, value > 4);
	}

	void init()
	{
		writeCommand(0b1, 0b00, 0b00000001); // SysOn
		writeCommand(0b1, 0b00, 0b00000011); // LCDon

		for (uint8_t i = 0; i <= 0x1F; i++)
			writeData(0b1, 0b01, i, 0b0000);
	}
};

#endif // __TCP802_H_