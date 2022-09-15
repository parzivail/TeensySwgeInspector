#include <Adafruit_GPS.h>
#include <SD.h>
#include <SPI.h>
#include "display.h"
#include "packet.h"
#include "structio.h"

#define BYTE_START (0x7F)
#define BYTE_ESC (0x7E)
#define BYTE_END (0x7D)
#define BYTE_XOR (0x20)
#define BYTE_SEED (0xAA)

#define LCD_CK (3)
#define LCD_DI (4)
#define LCD_CS (5)

#define U_HOST (Serial)
#define U_GPS (Serial1)
#define U_RADIO37 (Serial2)
#define U_RADIO38 (Serial3)
#define U_RADIO39 (Serial5)

#define RADIO_BAUD_RATE (115200)

#define ADVERTISING_RADIO_ACCESS_ADDRESS (0x8E89BED6)
#define ADVERTISING_CRC_INIT (0x555555)

#define SERIAL_BUFFER_SIZE (65536)
static DMAMEM uint8_t RADIO37_RX_BUFFER[SERIAL_BUFFER_SIZE] = {0};
static DMAMEM uint8_t RADIO38_RX_BUFFER[SERIAL_BUFFER_SIZE] = {0};
static DMAMEM uint8_t RADIO39_RX_BUFFER[SERIAL_BUFFER_SIZE] = {0};

#define GPS_BUFFER_SIZE (16384)
static DMAMEM uint8_t GPS_RX_BUFFER[GPS_BUFFER_SIZE] = {0};
Adafruit_GPS GPS(&U_GPS);

Display display(LCD_CK, LCD_DI, LCD_CS);

Sd2Card card;
SdVolume volume;
SdFile root;

File dumpFile;

uint64_t packetCount = 0;
static DMAMEM uint8_t packet_buffer[65536] = {0};
uint64_t rollingPacketCount = 0;

uint64_t fileSizeCounter = 0;
uint64_t lastFlush = 0;
uint64_t lastMillisNoted = 0;
uint64_t lastDisplayUpdate = 0;

enum
{
	OUTPUT_TYPE_SYSTEM_TIMESTAMP = 0x00,
	OUTPUT_TYPE_NMEA_SENTENCE = 0x01,
	OUTPUT_TYPE_RADIO_PACKET_37 = 0x02,
	OUTPUT_TYPE_RADIO_PACKET_38 = 0x03,
	OUTPUT_TYPE_RADIO_PACKET_39 = 0x04,
};

void printTagName(Stream &stream, int tag)
{
	if (tag == 0)
	{
		stream.print("TAG_DATA");
		return;
	}
	if (tag == 0x40)
	{
		stream.print("TAG_MSG_RESET_COMPLETE");
		return;
	}
	if (tag == 0x41)
	{
		stream.print("TAG_MSG_CONNECT_REQUEST");
		return;
	}
	if (tag == 0x42)
	{
		stream.print("TAG_MSG_CONNECTION_EVENT");
		return;
	}
	if (tag == 0x43)
	{
		stream.print("TAG_MSG_CONN_PARAM_UPDATE");
		return;
	}
	if (tag == 0x44)
	{
		stream.print("TAG_MSG_CHAN_MAP_UPDATE");
		return;
	}
	if (tag == 0x50)
	{
		stream.print("TAG_MSG_LOG");
		return;
	}
	if (tag == 0x45)
	{
		stream.print("TAG_MSG_TERMINATE");
		return;
	}
	if (tag == 0x80)
	{
		stream.print("TAG_CMD_RESET");
		return;
	}
	if (tag == 0x81)
	{
		stream.print("TAG_CMD_GET_VERSION");
		return;
	}
	if (tag == 0x82)
	{
		stream.print("TAG_CMD_SNIFF_CHANNEL");
		return;
	}

	stream.print("<Unknown Tag 0x");
	stream.print(tag, HEX);
	stream.print(">");
}

int timedRead(Stream &stream, uint32_t timeout)
{
	int c;
	unsigned long startMillis = millis();
	do
	{
		c = stream.read();
		if (c >= 0)
			return c;
		yield();
	} while (millis() - startMillis < timeout);
	return -1; // -1 indicates timeout
}

bool consumeFrameBegin(HardwareSerial &radio)
{
	return radio.read() == BYTE_START;
}

int32_t consumeFrame(HardwareSerial &radio)
{
	uint16_t length = 0;
	uint8_t checksum = BYTE_SEED;

	while (true)
	{
		int32_t data = timedRead(radio, 100);
		if (data == -1)
			return -1;

		if (data == BYTE_ESC)
		{
			data = timedRead(radio, 100);
			if (data == -1)
				return -1;
			data ^= BYTE_XOR;
		}
		else if (data == BYTE_END)
			break;

		packet_buffer[length] = data;
		checksum ^= data;

		length++;
	}

	// Pop the checksum byte off the top
	length--;

	// The checksum byte is at the end of the frame stream
	// so if the checksum was computed from the preceding
	// correctly, it will XOR with itself and become zero
	if (checksum != 0)
		return -1;

	return length;
}

void printVersion(HardwareSerial &radio)
{
	packet_header_t phCheckVersion;
	phCheckVersion.tag = TAG_CMD_GET_VERSION;
	phCheckVersion.length = 0;
	writeStruct(radio, &phCheckVersion);
	radio.flush();

	phCheckVersion.tag = 0xFF;
	phCheckVersion.length = 0xFFFF;

	if (!readStruct(radio, &phCheckVersion))
	{
		U_HOST.print("End of stream");
		return;
	}

	if (phCheckVersion.tag == TAG_CMD_GET_VERSION)
	{
		while (radio.available() < phCheckVersion.length)
			;
		for (auto i = 0; i < phCheckVersion.length; i++)
			U_HOST.write(radio.read());
	}
	else
		U_HOST.println("Bad packet tag");
}

void resetRadio(HardwareSerial &radio)
{
	packet_header_t header;
	header.tag = TAG_CMD_RESET;
	header.length = 0;
	writeStruct(radio, &header);

	radio.flush();
	radio.clear();
}

void startSniffer(HardwareSerial &radio, uint8_t channel)
{
	packet_header_t phStartSniffer;
	phStartSniffer.tag = TAG_CMD_SNIFF_CHANNEL;
	phStartSniffer.length = 20;

	uint8_t empty_mac[BDADDR_SIZE] = {0};

	msg_t mStartSniffer;
	mStartSniffer.timestamp = 0;
	mStartSniffer.data.cmd_sniff_channel.channel = channel;
	mStartSniffer.data.cmd_sniff_channel.aa = ADVERTISING_RADIO_ACCESS_ADDRESS;
	mStartSniffer.data.cmd_sniff_channel.crc_init = ADVERTISING_CRC_INIT;
	memcpy(mStartSniffer.data.cmd_sniff_channel.mac, empty_mac, BDADDR_SIZE);
	mStartSniffer.data.cmd_sniff_channel.rssi_min_negative = 0xFF;

	packet_t pStartSniffer;
	pStartSniffer.header = phStartSniffer;
	pStartSniffer.msg = mStartSniffer;
	writeStruct(radio, &pStartSniffer, 23);

	radio.flush();
	radio.clear();
}

void processPacket(int32_t frameLength)
{
	if (frameLength == -1)
		return;

	packet_t *packet = (packet_t *)packet_buffer;
	if (packet->header.tag == TAG_DATA)
	{
		// TODO: do something with MAC addresses?

		// auto packet = (radio_t *)packet_buffer;
		// U_HOST.print(", time: ");
		// U_HOST.print(packet->timestamp);
		// U_HOST.print(", channel: ");
		// U_HOST.print(packet->channel);
		// U_HOST.print(", rssi: ");
		// U_HOST.print(-packet->rssi_negative);

		// U_HOST.println();
	}
}

void setup()
{
	// Announce boot
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWriteFast(LED_BUILTIN, HIGH);

	display.init();
	display.setStatus("Checking logs");

	// Print crash report, if any
	if (CrashReport)
	{
		display.setStatus("Found crash");
		for (uint8_t i = 0; i < 100; i++)
		{
			U_HOST.println("Waiting");
			delay(50);
		}
		U_HOST.println(CrashReport);
	}

	display.setStatus("Init SD card");

	// Initialize SD card
	if (!SD.begin(BUILTIN_SDCARD))
	{
		display.setStatus("No SD card");
		U_HOST.println("SD::begin failed");
		return;
	}

	// Find next available filename
	char filename[9];
	uint16_t fileIdx = 0;
	do
	{
		fileIdx++;
		sprintf(filename, "%04d.bin", fileIdx);
	} while (SD.exists(filename));

	U_HOST.print("Output file: ");
	U_HOST.println(filename);

	dumpFile = SD.open(filename, FILE_WRITE_BEGIN);

	/*
		RADIO37
			P15 -  7 (RX2)
			P16 -  8 (TX2)
		RADIO38
			P15 - 15 (RX3)
			P16 - 14 (TX3)
		RADIO39
			P15 - 21 (RX5)
			P16 - 20 (TX5)
	*/

	display.setStatus("Init radios");

	// Initialize radio UART
	U_RADIO37.addMemoryForRead(&RADIO37_RX_BUFFER, SERIAL_BUFFER_SIZE);
	U_RADIO37.setTimeout(0x7FFFFFFF);
	U_RADIO37.begin(RADIO_BAUD_RATE);

	U_RADIO38.addMemoryForRead(&RADIO38_RX_BUFFER, SERIAL_BUFFER_SIZE);
	U_RADIO38.setTimeout(0x7FFFFFFF);
	U_RADIO38.begin(RADIO_BAUD_RATE);

	U_RADIO39.addMemoryForRead(&RADIO39_RX_BUFFER, SERIAL_BUFFER_SIZE);
	U_RADIO39.setTimeout(0x7FFFFFFF);
	U_RADIO39.begin(RADIO_BAUD_RATE);

	display.setStatus("Init GPS");

	// Initialize GPS UART
	U_GPS.addMemoryForRead(&GPS_RX_BUFFER, GPS_BUFFER_SIZE);
	GPS.begin(9600);
	GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGAGSA); // RMC (recommended minimum data), GGA (fix data), and GSA (fix metadata) packets
	GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

	display.setStatus("Start radios");

	// Reset radios
	resetRadio(U_RADIO37);
	resetRadio(U_RADIO38);
	resetRadio(U_RADIO39);
	delay(50);

	// Start radios
	startSniffer(U_RADIO37, 37);
	startSniffer(U_RADIO38, 38);
	startSniffer(U_RADIO39, 39);
	delay(50);

	display.setStatus(filename);

	digitalWriteFast(LED_BUILTIN, LOW);
}

/*
	Loop tasks:
		Flush the file buffer at regular intervals
		Tally and print the radio packet statistics
		Read zero or one sentence from the GPS, write to output
		Read one radio packet from each radio, write to output
*/
void loop()
{
	uint32_t now = millis();
	uint16_t nowMicrosFraction = micros() % 1000;

	// Write system timestamp
	if (now - lastMillisNoted > 250)
	{
		dumpFile.write(OUTPUT_TYPE_SYSTEM_TIMESTAMP);
		dumpFile.write((uint8_t *)&now, 4);
		dumpFile.write((uint8_t *)&nowMicrosFraction, 2);
		fileSizeCounter += 7;
		lastMillisNoted = now;
	}

	// Display statistics
	if (now - lastFlush > 10000)
	{
		U_HOST.print(packetCount);
		U_HOST.println(" packets");

		U_HOST.flush();
		dumpFile.flush();

		lastFlush = now;
	}

	// Update LCD
	if (now - lastDisplayUpdate > 1000)
	{
		display.setDetailsCount(packetCount, rollingPacketCount, fileSizeCounter * 8);

		fileSizeCounter = 0;
		rollingPacketCount = 0;

		lastDisplayUpdate = now;
	}

	if (consumeFrameBegin(U_RADIO37))
	{
		now = millis();
		nowMicrosFraction = micros() % 1000;
		
		int32_t frameLength = consumeFrame(U_RADIO37);
		if (frameLength != -1)
		{
			processPacket(frameLength);

			dumpFile.write(OUTPUT_TYPE_RADIO_PACKET_37);
			dumpFile.write((uint8_t *)&now, 4);
			dumpFile.write((uint8_t *)&nowMicrosFraction, 2);
			dumpFile.write((uint8_t *)&frameLength, 4);
			dumpFile.write(packet_buffer, frameLength);

			packetCount++;
			rollingPacketCount++;
			fileSizeCounter += frameLength + 11;
		}
	}

	if (consumeFrameBegin(U_RADIO38))
	{
		now = millis();
		nowMicrosFraction = micros() % 1000;

		int32_t frameLength = consumeFrame(U_RADIO38);
		if (frameLength != -1)
		{
			processPacket(frameLength);

			dumpFile.write(OUTPUT_TYPE_RADIO_PACKET_38);
			dumpFile.write((uint8_t *)&now, 4);
			dumpFile.write((uint8_t *)&nowMicrosFraction, 2);
			dumpFile.write((uint8_t *)&frameLength, 4);
			dumpFile.write(packet_buffer, frameLength);

			packetCount++;
			rollingPacketCount++;
			fileSizeCounter += frameLength + 11;
		}
	}

	if (consumeFrameBegin(U_RADIO39))
	{
		now = millis();
		nowMicrosFraction = micros() % 1000;
		
		int32_t frameLength = consumeFrame(U_RADIO39);
		if (frameLength != -1)
		{
			processPacket(frameLength);

			dumpFile.write(OUTPUT_TYPE_RADIO_PACKET_39);
			dumpFile.write((uint8_t *)&now, 4);
			dumpFile.write((uint8_t *)&nowMicrosFraction, 2);
			dumpFile.write((uint8_t *)&frameLength, 4);
			dumpFile.write(packet_buffer, frameLength);

			packetCount++;
			rollingPacketCount++;
			fileSizeCounter += frameLength + 11;
		}
	}

	// Read one sentence from the GPS
	if (U_GPS.available())
	{
		GPS.read();
		if (GPS.newNMEAreceived())
		{
			auto sentence = GPS.lastNMEA();
			auto sentenceLength = strlen(sentence);
			if (sentenceLength <= 0xFF)
			{
				now = millis();
				nowMicrosFraction = micros() % 1000;

				sentenceLength &= 0xFF;

				dumpFile.write(OUTPUT_TYPE_NMEA_SENTENCE);
				dumpFile.write((uint8_t *)&now, 4);
				dumpFile.write((uint8_t *)&nowMicrosFraction, 2);
				dumpFile.write((uint8_t)sentenceLength);
				dumpFile.write(sentence, sentenceLength);

				fileSizeCounter += sentenceLength + 8;

				if (GPS.parse(sentence))
				{
					// TODO: do something with parsed GPS info?
				}
			}
		}
	}
}