#include <SD.h>
#include <SPI.h>
#include "packet.h"
#include "structio.h"

#define U_HOST (Serial)
#define U_RADIO37 (Serial2)
#define U_RADIO38 (Serial3)
#define U_RADIO39 (Serial4)

#define RADIO_BAUD_RATE (1000000)

#define ADVERTISING_RADIO_ACCESS_ADDRESS (0x8E89BED6)
#define ADVERTISING_CRC_INIT (0x555555)

#define SERIAL_BUFFER_SIZE (16384)
static uint8_t RADIO37_RX_BUFFER[SERIAL_BUFFER_SIZE] = {0};

Sd2Card card;
SdVolume volume;
SdFile root;

File dumpFile;

uint64_t packetCount = 0;
uint8_t packet_buffer[1024] = {0};
uint64_t rollingPacketCount = 0;

uint64_t fileSizeCounter = 0;
uint64_t lastFileSizeCount = 0;

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

void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWriteFast(LED_BUILTIN, HIGH);
	delay(1000);
	digitalWriteFast(LED_BUILTIN, LOW);

	if (CrashReport)
		U_HOST.println(CrashReport);

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

	char filename[9];
	auto fileIdx = 0;
	do
	{
		fileIdx++;
		sprintf(filename, "%04d.bin", fileIdx);
	} while (SD.exists(filename));

	dumpFile = SD.open(filename, FILE_WRITE_BEGIN);

	U_RADIO37.addMemoryForRead(&RADIO37_RX_BUFFER, SERIAL_BUFFER_SIZE);
	U_RADIO37.setTimeout(0x7FFFFFFF);
	U_RADIO37.begin(RADIO_BAUD_RATE);
	U_RADIO37.attachCts(6);
	U_RADIO37.attachRts(9);
	delay(50);

	resetRadio(U_RADIO37);
	delay(50);

	U_HOST.print("Output file: ");
	U_HOST.println(filename);
	U_HOST.print("Radio version: ");
	printVersion(U_RADIO37);
	U_HOST.println();
	delay(50);

	startSniffer(U_RADIO37, 37);
	delay(50);
}

void loop()
{
	if (packetCount > 0 && packetCount % 1024 == 0)
	{
		U_HOST.println("--- Flush ---");
		U_HOST.flush();
		dumpFile.flush();
	}

	auto now = millis();
	if (now - lastFileSizeCount > 5000)
	{
		U_HOST.print(packetCount);
		U_HOST.print(" packets, ");
		U_HOST.print(rollingPacketCount / 5);
		U_HOST.print(" packets/s, ");
		U_HOST.print(fileSizeCounter * 8 / 5);
		U_HOST.println(" bps");

		lastFileSizeCount = now;
		fileSizeCounter = 0;
		rollingPacketCount = 0;
	}

	packet_header_t packetHeader = {0};
	readStruct(U_RADIO37, &packetHeader);

	if (packetHeader.tag != TAG_DATA)
	{
		U_HOST.print(packetCount);
		U_HOST.print(" type: ");
		printTagName(U_HOST, packetHeader.tag);
	}

	U_RADIO37.readBytes(packet_buffer, packetHeader.length);

	uint32_t timestamp = millis();
	dumpFile.write((uint8_t *)&timestamp, 4);
	dumpFile.write((uint8_t *)&packetHeader, 3);
	dumpFile.write(packet_buffer, packetHeader.length);

	fileSizeCounter += 3;
	fileSizeCounter += packetHeader.length;

	if (packetHeader.tag == TAG_DATA)
	{
		// auto packet = (radio_t *)packet_buffer;
		// U_HOST.print(", time: ");
		// U_HOST.print(packet->timestamp);
		// U_HOST.print(", channel: ");
		// U_HOST.print(packet->channel);
		// U_HOST.print(", rssi: ");
		// U_HOST.print(-packet->rssi_negative);

		// U_HOST.println();
	}
	else if (packetHeader.tag == TAG_MSG_CONNECTION_EVENT)
	{
		auto packet = (msg_t *)packet_buffer;
		U_HOST.print(", time: ");
		U_HOST.print(packet->timestamp);
		U_HOST.print(", count: ");
		U_HOST.print(packet->data.connection_event.count);

		U_HOST.println();
	}
	else if (packetHeader.tag == TAG_MSG_TERMINATE)
	{
		auto packet = (msg_t *)packet_buffer;
		U_HOST.print(", time: ");
		U_HOST.print(packet->timestamp);
		U_HOST.print(", reason: ");
		U_HOST.print(packet->data.terminate.reason);

		U_HOST.println();
	}
	else if (packetHeader.tag == TAG_MSG_CONNECT_REQUEST)
	{
		auto packet = (msg_t *)packet_buffer;
		U_HOST.print(", time: ");
		U_HOST.print(packet->timestamp);

		U_HOST.println();
	}
	else if (packetHeader.tag == TAG_MSG_LOG)
	{
		auto packet = (msg_t *)packet_buffer;
		U_HOST.print(", time: ");
		U_HOST.print(packet->timestamp);
		U_HOST.print(", message: ");
		U_HOST.write(packet->data.value, packetHeader.length - 4);

		U_HOST.println();
	}

	packetCount++;
	rollingPacketCount++;
}