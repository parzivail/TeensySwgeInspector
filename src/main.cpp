#include <Adafruit_GPS.h>
#include <SD.h>
#include <SPI.h>
#include "packet.h"
#include "structio.h"

#define U_HOST (Serial)
#define U_GPS (Serial1)
#define U_RADIO37 (Serial2)
#define U_RADIO38 (Serial3)
#define U_RADIO39 (Serial4)

#define RADIO_BAUD_RATE (1000000)

#define ADVERTISING_RADIO_ACCESS_ADDRESS (0x8E89BED6)
#define ADVERTISING_CRC_INIT (0x555555)

#define SERIAL_BUFFER_SIZE (16384)
static uint8_t RADIO37_RX_BUFFER[SERIAL_BUFFER_SIZE] = {0};

static uint8_t GPS_RX_BUFFER[SERIAL_BUFFER_SIZE] = {0};
Adafruit_GPS GPS(&U_GPS);

Sd2Card card;
SdVolume volume;
SdFile root;

File dumpFile;

uint64_t packetCount = 0;
uint8_t packet_buffer[1024] = {0};
uint64_t rollingPacketCount = 0;

uint64_t fileSizeCounter = 0;
uint64_t lastFileSizeCount = 0;

enum
{
	OUTPUT_TYPE_SYSTEM_TIMESTAMP = 0x00,
	OUTPUT_TYPE_NMEA_SENTENCE = 0x01,
	OUTPUT_TYPE_RADIO_PACKET_37 = 0x02,
	OUTPUT_TYPE_RADIO_PACKET_38 = 0x03,
	OUTPUT_TYPE_RADIO_PACKET_39 = 0x04,
};

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

void setup()
{
	// Announce boot
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWriteFast(LED_BUILTIN, HIGH);

	// Print crash report, if any
	// if (CrashReport)
	// 	U_HOST.println(CrashReport);

	// Initialize SD card
	if (!card.init(SPI_FULL_SPEED, BUILTIN_SDCARD))
	{
		U_HOST.println("card::init failed");
		return;
	}

	if (!volume.init(card))
	{
		U_HOST.println("volume::init failed");
		return;
	}

	// Find next available filename
	char filename[9];
	auto fileIdx = 0;
	do
	{
		fileIdx++;
		sprintf(filename, "%04d.bin", fileIdx);
	} while (SD.exists(filename));

	U_HOST.print("Output file: ");
	U_HOST.println(filename);

	dumpFile = SD.open(filename, FILE_WRITE_BEGIN);

	// Initialize radio UART
	U_RADIO37.addMemoryForRead(&RADIO37_RX_BUFFER, SERIAL_BUFFER_SIZE);
	U_RADIO37.setTimeout(0x7FFFFFFF);
	U_RADIO37.begin(RADIO_BAUD_RATE);
	U_RADIO37.attachCts(6);
	U_RADIO37.attachRts(9);

	// Initialize GPS UART
	U_GPS.addMemoryForRead(&GPS_RX_BUFFER, SERIAL_BUFFER_SIZE);
	GPS.begin(9600);
	GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGAGSA); // RMC (recommended minimum data), GGA (fix data), and GSA (fix metadata) packets
	GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

	// Reset radios
	resetRadio(U_RADIO37);
	delay(50);

	// Start radios
	startSniffer(U_RADIO37, 37);
	delay(50);

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
	// Display statistics
	auto now = millis();
	if (now - lastFileSizeCount > 10000)
	{
		U_HOST.print(packetCount);
		U_HOST.print(" packets, ");
		U_HOST.print(rollingPacketCount / 10);
		U_HOST.print(" packets/s, ");
		U_HOST.print(fileSizeCounter * 8 / 10);
		U_HOST.println(" bps");

		U_HOST.flush();
		dumpFile.flush();

		lastFileSizeCount = now;
		fileSizeCounter = 0;
		rollingPacketCount = 0;
	}

	// Write system timestamp
	uint32_t timestamp = millis();
	dumpFile.write(OUTPUT_TYPE_SYSTEM_TIMESTAMP);
	dumpFile.write((uint8_t *)&timestamp, 4);
	fileSizeCounter += 5;

	// Read one packet from Radio37
	packet_header_t packetHeader = {0};
	readStruct(U_RADIO37, &packetHeader);
	U_RADIO37.readBytes(packet_buffer, packetHeader.length);

	dumpFile.write(OUTPUT_TYPE_RADIO_PACKET_37);
	dumpFile.write((uint8_t *)&packetHeader, 3);
	dumpFile.write(packet_buffer, packetHeader.length);

	packetCount++;
	rollingPacketCount++;
	fileSizeCounter += 4 + packetHeader.length;

	// Keep statistucs about RX'd packets
	if (packetHeader.tag == TAG_DATA)
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

	// Read one sentence from the GPS
	while (U_GPS.available())
	{
		GPS.read();
		if (GPS.newNMEAreceived())
		{
			auto sentence = GPS.lastNMEA();
			auto sentenceLength = strlen(sentence);
			if (sentenceLength <= 0xFF)
			{
				sentenceLength &= 0xFF;

				dumpFile.write(OUTPUT_TYPE_NMEA_SENTENCE);
				dumpFile.write((uint8_t)sentenceLength);
				dumpFile.write(sentence, sentenceLength);

				fileSizeCounter += sentenceLength + 2;

				if (GPS.parse(sentence))
				{
					// TODO: do something with parsed GPS info?
				}
			}
		}
	}
}