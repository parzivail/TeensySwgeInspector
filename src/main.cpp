#include <SD.h>
#include <SPI.h>
#include "packet.h"
#include "structio.h"

Sd2Card card;
SdVolume volume;
SdFile root;

#define U_HOST (Serial)
#define U_RADIO37 (Serial2)
#define U_RADIO38 (Serial3)
#define U_RADIO39 (Serial4)

#define RADIO_BAUD_RATE (1000000)
#define HOST_BAUD_RATE (1000000)

#define ADVERTISING_RADIO_ACCESS_ADDRESS (0x8E89BED6)
#define ADVERTISING_CRC_INIT (0x555555)

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

void startSniffer(Stream &radio)
{
	packet_header_t phStartSniffer;
	phStartSniffer.tag = TAG_CMD_SNIFF_CHANNEL;
	phStartSniffer.length = 20;

	uint8_t empty_mac[6] = {0};

	msg_t mStartSniffer;
	mStartSniffer.timestamp = 0;
	mStartSniffer.data.cmd_sniff_channel.channel = 37;
	mStartSniffer.data.cmd_sniff_channel.aa = ADVERTISING_RADIO_ACCESS_ADDRESS;
	mStartSniffer.data.cmd_sniff_channel.crc_init = ADVERTISING_CRC_INIT;
	memcpy(mStartSniffer.data.cmd_sniff_channel.mac, empty_mac, 6);
	mStartSniffer.data.cmd_sniff_channel.rssi_min_negative = 0xFF;

	packet_t pStartSniffer;
	pStartSniffer.header = phStartSniffer;
	pStartSniffer.msg = mStartSniffer;
	writeStruct(radio, &pStartSniffer, 23);
	radio.flush();
}

void setup()
{
	U_RADIO37.begin(RADIO_BAUD_RATE);
	U_RADIO37.attachCts(6);
	U_RADIO37.attachRts(9);

	delay(500);

	// if (!card.init(SPI_FULL_SPEED, BUILTIN_SDCARD))
	// {
	// 	U_HOST.println("card::init failed");
	// 	return;
	// }

	// if (!volume.init(card))
	// {
	// 	U_HOST.println("volume::init failed");
	// 	return;
	// }

	// U_HOST.begin(HOST_BAUD_RATE);
	// while (!U_HOST)
	// 	;

	// U_HOST.print("Radio version: ");
	// printVersion(U_RADIO37);
	// U_HOST.println();

	startSniffer(U_RADIO37);

	delay(500);
}

void loop()
{
	while (U_RADIO37.available() > 0)
		U_HOST.write(U_RADIO37.read());
	while (U_HOST.available() > 0)
		U_RADIO37.write(U_HOST.read());
}