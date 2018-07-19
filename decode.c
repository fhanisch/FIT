/*
	cl /nologo /W4 decode.c
	clang -Wall decode.c -o decode.exe
*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef enum {
	MESSAGE_TYPE_SPECIFIC_BIT = 0x20,
	MESSAGE_TYPE_BIT = 0x40,
	NORMAL_HEADER_BIT = 0x80,
	LOCAL_MESSAGE_TYPE_BITS = 0xF
} NormalHeaderFlagBits;

typedef enum {
	HEART_RATE = 3,
	DISTANCE = 5,
	SPEED = 6,
	TIMESTAMP = 253
} RecordFields;

typedef struct {
	unsigned char headerSize;
	unsigned char protocolVersion;
	unsigned short profileVersion;
	unsigned int dataSize;
	char formatExtension[4];
	unsigned short crc;
} Header;

typedef struct {
	unsigned char normalHeader;
	unsigned char messageType;
	unsigned char messageTypeSpecific;
	unsigned char localMessageType;
} NormalHeader;

typedef struct {
	unsigned char fieldDefNr;
	unsigned char size;
	unsigned char baseType;
} FieldDef;

typedef struct {
	unsigned short globalMsgNr;
	unsigned char fieldCount;
	FieldDef *fieldDefs;
} DefMsg;

NormalHeader readNormalHeader(FILE *file) {
	unsigned char byte;
	NormalHeader normalHeader;
	fread(&byte, 1, 1, file);
	if (byte & NORMAL_HEADER_BIT) normalHeader.normalHeader = 1;
	else normalHeader.normalHeader = 0;
	if (byte & MESSAGE_TYPE_BIT) normalHeader.messageType = 1;
	else normalHeader.messageType = 0;
	if (byte & MESSAGE_TYPE_SPECIFIC_BIT) normalHeader.messageTypeSpecific = 1;
	else normalHeader.messageTypeSpecific = 0;
	normalHeader.localMessageType = byte & LOCAL_MESSAGE_TYPE_BITS;
	/*
	printf("Normal Header:         %d\n", normalHeader.normalHeader);
	printf("Message Type:          %d\n", normalHeader.messageType);
	printf("Message Type Specific: %d\n", normalHeader.messageTypeSpecific);
	printf("Local Message Type:    %d\n", normalHeader.localMessageType);
	printf("\n");
	*/
	return normalHeader;
}

int main(int argc, char *argv[]) {
	Header header;
	NormalHeader  normal;
	unsigned char localMsg;
	DefMsg defMsg[16];
	unsigned char buf[256];

	if (argc < 2) {
		printf("usage: decode.exe <fit file>\n");
		return 1;
	}
	FILE *file = fopen(argv[1], "rb");
	printf("Size of unsigned short: %llu\n", sizeof(unsigned short));
	printf("Size of Header: %llu\n\n", sizeof(Header));

	fread(&header, 14, 1, file);
	printf("HeaderSize:       %u\n", header.headerSize);
	printf("Protocol Version: %u\n", header.protocolVersion);
	printf("Profile Version:  %u\n", header.profileVersion);
	printf("Data Size:        %u\n", header.dataSize);
	strcpy((char*)buf, header.formatExtension);
	buf[4] = 0;
	printf("Format Extension: '%s'\n", buf);
	printf("\n");

	unsigned int refTime = 1111;
	unsigned int check = 0;
	unsigned char sec;
	unsigned char min;
	unsigned char hours;
	unsigned short gapCount = 0;
	for (int j = 0; j < 7000; j++) {
		normal = readNormalHeader(file);
		if (normal.normalHeader) {
			printf("Compressed Timestamp Header\n");
			return 2;
		}
		if (normal.messageTypeSpecific) {
			printf("Specific Message Type\n");
			return 3;
		}
		localMsg = normal.localMessageType;
		if (normal.messageType) {
			fread(buf, 5, 1, file);
			defMsg[localMsg].globalMsgNr = *(unsigned short*)(&buf[2]);
			defMsg[localMsg].fieldCount = buf[4];
			/*
			printf("Architecture:          %u\n", buf[1]);
			printf("Global Message Number: %u\n", defMsg[localMsg].globalMsgNr);
			printf("Number of Fields:      %u\n", defMsg[localMsg].fieldCount);
			printf("\n");
			*/
			defMsg[localMsg].fieldDefs = malloc(defMsg[localMsg].fieldCount * 3);
			fread(defMsg[localMsg].fieldDefs, defMsg[localMsg].fieldCount * 3, 1, file);
		}
		else {
			for (int i = 0; i < defMsg[localMsg].fieldCount; i++) {
				/*
				printf("Field #%d:\n", i);
				printf("\tField Number: %u\n", defMsg[localMsg].fieldDefs[i].fieldDefNr);
				printf("\tSize:         %u\n", defMsg[localMsg].fieldDefs[i].size);
				printf("\tBase Type:    0x%x\n", defMsg[localMsg].fieldDefs[i].baseType);
				*/
				fread(buf, defMsg[localMsg].fieldDefs[i].size, 1, file);
				if (defMsg[localMsg].globalMsgNr == 20) {
					switch (defMsg[localMsg].fieldDefs[i].fieldDefNr) {
					case HEART_RATE: {
						printf("Heart Rate: %4u bpm\n", buf[0]);
						break;
					}
					case SPEED: {
						printf("Speed: %6.2f km/h    ", (double)(*(unsigned short*)&buf[0]) / 1000.0*3.6);
						break;
					}
					case DISTANCE: {
						printf("Distance: %10.2f m    ", (double)(*(unsigned int*)&buf[0]) / 100.0);
						break;
					}
					case TIMESTAMP: {
						if (refTime == 1111) {
							refTime = *(unsigned int*)&buf[0];
							check = refTime;
						}
						if (*(unsigned int*)&buf[0] - check > 1) gapCount++;
						check = *(unsigned int*)&buf[0];
						sec = (unsigned char)((*(unsigned int*)&buf[0] - refTime) % 60);
						min = (unsigned char)(((*(unsigned int*)&buf[0] - refTime) / 60) % 60);
						hours = (unsigned char)(((*(unsigned int*)&buf[0] - refTime) / 60) / 60);
						printf("Time: %02u:%02u:%02u    ", hours, min, sec);
						break;
					}
					}
				}
			}
		}
	}
	printf("Gap Count: %u\n", gapCount);
	fclose(file);

	return 0;
}