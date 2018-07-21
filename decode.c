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
	SESSION = 18,
	LAP = 19,
	RECORD = 20
} MESG_NUM;

typedef enum {
	HEART_RATE = 3,
	DISTANCE = 5,
	SPEED = 6,
	TIMESTAMP = 253
} EnumRecordFields;

typedef enum {
	MESSAGE_INDEX = 254,
	TOTAL_ELAPSED_TIME = 7,
	TOTAL_DISTANCE = 9,
	AVG_SPEED = 14,
	MAX_SPEED = 15,
	AVG_HEART_RATE = 16,
	MAX_HEART_RATE = 17
} EnumSessionFields;

typedef struct {
	unsigned short index;
	unsigned int time; // [s]
	double distance; // [km]
	double avgSpeed; // [km/h]
	double maxSpeed; // [km/h]
	unsigned char avgHR; // [bpm]
	unsigned char maxHR; // [bpm]
} SessionFields;

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

typedef struct {
	unsigned char sec;
	unsigned char min;
	unsigned char hours;
} Time;

typedef struct {
	unsigned char delta;
	unsigned int timeStamp;
} GapInfo;

size_t numBytes = 0;

Time getTime(unsigned int t) {
	Time time;
	time.sec = (unsigned char)(t % 60);
	time.min = (unsigned char)((t / 60) % 60);
	time.hours = (unsigned char)((t / 60) / 60);
	return time;
}

NormalHeader readNormalHeader(FILE *file) {
	unsigned char byte;
	NormalHeader normalHeader;
	fread(&byte, 1, 1, file);
	numBytes++;
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
	int fileSize;

	if (argc < 2) {
		printf("usage: decode.exe <fit file>\n");
		return 1;
	}
	FILE *file = fopen(argv[1], "rb");
	printf("Size of unsigned short: %llu\n", sizeof(unsigned short));
	printf("Size of Header: %llu\n\n", sizeof(Header));
	fseek(file, 0, SEEK_END);
	fileSize = ftell(file);
	rewind(file);
	printf("File Size:        %d\n\n", fileSize);

	fread(&header, 14, 1, file);
	numBytes += 14;
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
	Time time;
	GapInfo gap[256];
	unsigned char gapCount = 0;
	SessionFields sessionFields = {1111,1111,-1,-1,-1,0,0};
	while (numBytes < fileSize-2) {
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
			numBytes += 5;
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
			numBytes += defMsg[localMsg].fieldCount * 3;
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
				numBytes += defMsg[localMsg].fieldDefs[i].size;
				switch (defMsg[localMsg].globalMsgNr) {
					case RECORD: {
						switch (defMsg[localMsg].fieldDefs[i].fieldDefNr) {
							case HEART_RATE: {
								printf("Heart Rate: %4u bpm\n", buf[0]);
								break;
							}
							case SPEED: {
								printf("Speed: %6.2lf km/h    ", (double)(*(unsigned short*)&buf[0]) / 1000.0*3.6);
								break;
							}
							case DISTANCE: {
								printf("Distance: %10.2lf m    ", (double)(*(unsigned int*)&buf[0]) / 100.0);
								break;
							}
							case TIMESTAMP: {
								if (refTime == 1111) {
									refTime = *(unsigned int*)&buf[0];
									check = refTime;
								}
								if (*(unsigned int*)&buf[0] - check > 1) {
									gap[gapCount].delta = (unsigned char)(*(unsigned int*)&buf[0] - check);
									gap[gapCount].timeStamp = *(unsigned int*)&buf[0];
									gapCount++;
								}
								check = *(unsigned int*)&buf[0];
								time = getTime(*(unsigned int*)&buf[0] - refTime);
								printf("Time: %02u:%02u:%02u    ", time.hours, time.min, time.sec);
								break;
							}
						}
						break;
					}
					case SESSION: {
						switch (defMsg[localMsg].fieldDefs[i].fieldDefNr) {
							case MESSAGE_INDEX: {
								sessionFields.index = *(unsigned short*)&buf[0] & 0x0fff;
								break;
							}
							case TOTAL_ELAPSED_TIME: {
								sessionFields.time = *(unsigned int*)&buf[0] / 1000;
								break;
							}
							case TOTAL_DISTANCE: {
								sessionFields.distance = (double)(*(unsigned int*)&buf[0]) / 100000.0;
								break;
							}
							case AVG_SPEED: {
								sessionFields.avgSpeed = (double)(*(unsigned short*)&buf[0]) / 1000.0*3.6;
								break;
							}
							case MAX_SPEED: {
								sessionFields.maxSpeed = (double)(*(unsigned short*)&buf[0]) / 1000.0*3.6;
								break;
							}
							case AVG_HEART_RATE: {
								sessionFields.avgHR = buf[0];
								break;
							}
							case MAX_HEART_RATE: {
								sessionFields.maxHR = buf[0];
								break;
							}
						}
						break;
					}
				}
			}
		}
	}
	fclose(file);
	printf("\n");
	printf("Session:\n");
	time = getTime(sessionFields.time);
	printf("\tIndex:              %u\n", sessionFields.index);
	printf("\tTime:               %02u:%02u:%02u\n", time.hours, time.min, time.sec);
	printf("\tDistance:           %0.3lf km\n", sessionFields.distance);
	printf("\tAverage Speed:      %0.2lf km/h\n", sessionFields.avgSpeed);
	printf("\tMaximum Speed:      %0.2lf km/h\n", sessionFields.maxSpeed);
	printf("\tAverage Heart Rate: %u bpm\n", sessionFields.avgHR);
	printf("\tMaximum Heart Rate: %u bpm\n", sessionFields.maxHR);
	printf("\n");
	for (int i = 0; i < gapCount; i++) {
		printf("Gap #%u:\n", i+1);
		printf("\tDelta: %u sec\n", gap[i].delta);
		time = getTime(gap[i].timeStamp - refTime);
		printf("\tTime: %02u:%02u:%02u\n", time.hours, time.min, time.sec);
		printf("\n");
	}

	return 0;
}