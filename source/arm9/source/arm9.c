#include <nds.h>
#include <stdio.h>
#include <stdlib.h>

#include <fat.h>
#include <dirent.h>
#include <string.h>
#include "../../fwfifo.h"

#define OK_TO_DO_SOME_POTENTIALLY_DANGEROUS_STUFF 1

#if 0
u16 crc16_table[0x100] = {0}; /* not proper table */
u16 CRC16(u8* buffer, int size) /* replace with swiCRC16(0xFFFF, buffer, size); */
{
	u16 hash = 0xFFFF;
	u8 * end = buffer + size;
	do
	{
		u8 byte = *buffer++;
		byte ^= hash;
		hash = (hash >> 8) ^ crc16_table[byte]; // byte * sizeof(u16) - aka index
	} while (buffer != end);
	return hash;
}
#endif

inline int WAIT_FOR_BUTTON_PRESS(int keyMask)
{
	int key;
	do
	{
		scanKeys();
		key = keysDown();
	} while (!(key & keyMask));
	return key;
}

void fwSendMsg(int which, int offset, void* buffer, int size)
{
	FwFifoData msg;
	msg.page.fifo_cmd = which;
	msg.page.offset   = offset;
	msg.page.size     = size;
	msg.page.buffer   = buffer;
	if (!fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (void*)&msg))
	{
		iprintf("error sending msg\n");
		while (1) ;
	}
	int response;
	do
	{
		response = fifoCheckDatamsg(FIFO_USER_01);
	} while (response == 0);
	response = fifoGetDatamsg(FIFO_USER_01, sizeof(msg), (void*)&msg);
	if (response != 0x10)
	{
		iprintf("ack error\n");
		while (1) ;
	}
}

#define FW_SIZE 0x20000
u8 fw_buffer[FW_SIZE];
#define WORK_SIZE 0x20
u8 fw_work_buffer[WORK_SIZE];

void firmware_read(int offset, void* buffer, int size)
{
	fwSendMsg(FwRead, offset, buffer, size);
}

void firmware_program_and_write(int offset, void* buffer, int size)
{
	fwSendMsg(FwProgram, offset, buffer, size);
	fwSendMsg(FwWrite, offset, buffer, size);
}

u8 firmware_check_sr(void)
{
	fwSendMsg(FwReadSr, 0, fw_work_buffer, 1);
	return fw_work_buffer[0];
}

void firmware_send_msg7(u8 cmd)
{
	fw_work_buffer[0] = cmd;
	fwSendMsg(FwFoo, 0, fw_work_buffer, 1);
}

void programming(void)
{
	memset(memUncached(fw_work_buffer), 0, WORK_SIZE);
	int offset = 0;
	iprintf("  >> PROGRAMMING [");
	do
	{
		firmware_program_and_write(offset, fw_buffer + offset, 0x20);
		offset += 0x20;
		/* weird division function - used to show progress */
	} while (offset != FW_SIZE);
	iprintf("]\n");
}

int verifying(void)
{
	memset(memUncached(fw_work_buffer), 0, WORK_SIZE);
	int offset = 0;
	iprintf("  >> VERIFYING [");
	do
	{
		firmware_read(offset, fw_work_buffer, WORK_SIZE);
		if (memcmp(fw_work_buffer, fw_buffer + offset, WORK_SIZE))
		{
			iprintf("\n  >> VERIFY ERROR\n");
			WAIT_FOR_BUTTON_PRESS(KEY_A);
			/* call svc 5 */
			return -1;
		}
		offset += WORK_SIZE;
	} while (offset != FW_SIZE);
	iprintf("]\n");
	iprintf("  >> OK!\n");
	return 0;
}

char patches1[] = {
	0xb9,0xf2,0x10,0x00,0xae,0x2b,0x27,0x00,0xed,0x0d,0xdc,0xba,0x9c,0xf1,0x18,0x00,
	0x90,0xb6,0x10,0x00,0x00,0xb0,0xfa,0x00,0x00,0x02,0x20,0x00,0xb9,0xf2,0x10,0x00,
	0x00,0x90,0x27,0x00,0x01,0x00,0x00,0x00,0xe1,0x49,0x15,0x00,0x38,0x6f,0x27,0x00,
	0xac,0x82,0x1b,0x00,0xdc,0xd5,0x18,0x00,0x40,0x83,0x27,0x00,0x00,0x02,0x10,0x00,
	0xcc,0x48,0x00,0x00,0x60,0x3d,0x14,0x00,0xb9,0xf2,0x10,0x00,0x00,0x90,0x27,0x00,
	0x00,0x00,0x2b,0x00,0xf9,0x02,0x10,0x00,0xf9,0x02,0x10,0x00,0xf9,0x02,0x10,0x00,
	0xf9,0x02,0x10,0x00,0xf9,0x02,0x10,0x00,0xf9,0x02,0x10,0x00,0xe1,0x49,0x15,0x00,
	0x00,0x00,0x00,0x00,0xe1,0x49,0x15,0x00,0x20,0x90,0x27,0x00,0x8c,0x53,0x10,0x00,
	0x00,0x90,0x00,0x00,0x58,0x39,0x1b,0x00,0xe5,0x04,0x21,0x00,0x00,0xda,0x19,0x00,
	0x00,0x75,0x01,0x00,0x86,0xdf,0x21,0x00,0x00,0xc1,0x1a,0x00,0x22,0xda,0x1d,0x00,
	0x91,0xfe,0x16,0x00,0x00,0x01,0x10,0x00,0xbc,0x4c,0x14,0x00,0x00,0x00,0x2b,0x00,
	0x00,0x90,0x00,0x00,0xe1,0x49,0x15,0x00,0xac,0xef,0x22,0x00,0x88,0x5c,0x10,0x00,
	0x00,0x00,0x0e,0x00,0x90,0x03,0x25,0x00,0xc0,0xfa,0x1e,0x00,0x91,0xfe,0x16,0x00,
	0x8c,0x53,0x10,0x00,0x24,0x6b,0x03,0x00,0x60,0x3d,0x14,0x00,
};
int patches1_len = 0xDC;

char patches2[] = {
	0xb9,0xf2,0x10,0x00,0x00,0xfe,0x01,0x00,0x00,0x01,0x00,0x00,0xe1,0x49,0x15,0x00,
	0x00,0x94,0x27,0x00,0xfc,0x34,0x13,0x00,0xd0,0x8c,0x1e,0x00,0x8c,0x53,0x10,0x00,
	0x9c,0x94,0x27,0xf0,0x60,0x3d,0x14,0x00,
};
int patches2_len = 0x28;

void ClearScreen(void) { iprintf("\x1B[2J"); }
inline void INIT(void) { consoleDemoInit(); }

void dumpBufferToFile(char filename[], void* buffer)
{
	if (fatInitDefault())
	{
		FILE* file = fopen(filename, "wb");
		if(file)
		{
			if(fwrite(buffer, 1, FW_SIZE, file) == FW_SIZE)
			{
				iprintf("Wrote profile settings to file\n");
			}
			else
			{
				iprintf("Write failed\n");
			}
			fclose(file);
		}
		else
		{
			iprintf("fopen failed\n");
		}
	}
	else
	{
		iprintf("fatInitDefault failure: terminating\n");
	}
}

void printCRCs()
{
	iprintf("OrgCRC1 : %x \n", *(u16*)(fw_buffer + 0x1FE72));
	iprintf("GenCRC1 : %x \n\n", swiCRC16(0xFFFF, fw_buffer+0x1FE00, 0x70));

	iprintf("OrgCRC2 : %x \n", *(u16*)(fw_buffer + 0x1FEFE));
	iprintf("GenCRC2 : %x \n\n", swiCRC16(0xFFFF, fw_buffer+0x1FE74, 0x8A));

	iprintf("OrgCRC3 : %x \n", *(u16*)(fw_buffer + 0x1FF72));
	iprintf("GenCRC3 : %x \n\n", swiCRC16(0xFFFF, fw_buffer+0x1FF00, 0x70));

	iprintf("OrgCRC4 : %x \n", *(u16*)(fw_buffer + 0x1FFFE));
	iprintf("GenCRC4 : %x \n\n", swiCRC16(0xFFFF, fw_buffer+0x1FF74, 0x8A));
}

int main(int argc, char ** argv)
{
	void * uncached = memUncached(fw_buffer);
	memset(uncached, 0, FW_SIZE);

	INIT(); /* needs to setup console and fifo/ipc and buttons */
	ClearScreen();

	iprintf("\n\n  >> FAKEWAY 3DS INSTALLER\n\n");
	iprintf("  >> PRESS (A) TO INSTALL\n");
	iprintf("  >> PRESS (B) TO EXIT\n");
	int key = WAIT_FOR_BUTTON_PRESS(KEY_A|KEY_B);
	if (key & KEY_B) return EXIT_FAILURE;

	firmware_read(0, fw_buffer, FW_SIZE);
	dumpBufferToFile("settings.bin", fw_buffer);
	//printCRCs();
	/* DON'T GO PAST HERE WITHOUT VERIFYING READ WORKS */
#if OK_TO_DO_SOME_POTENTIALLY_DANGEROUS_STUFF
	memcpy(fw_buffer + 0x1FE00, patches1, patches1_len); /* UserSettings 1 */
	*(u16*)(fw_buffer + 0x1FE70) = 0x51; /* update_counter UserSettings 1 */
	*(u16*)(fw_buffer + 0x1FF70) = 0x52; /* update_counter UserSettings 2 */
	*(u16*)(fw_buffer + 0x1FF50) = 0x6E; /* message_length UserSettings 2 */
	memcpy(fw_buffer + 0x1FFB4, patches2, patches2_len); /* UserSettings 2 Not used area */

	/* PATCH CRC16s */
	*(u16*)(fw_buffer + 0x1FE72) = swiCRC16(0xFFFF, fw_buffer+0x1FE00, 0x70); /* 00h - 6Fh (1) */
	*(u16*)(fw_buffer + 0x1FEFE) = swiCRC16(0xFFFF, fw_buffer+0x1FE74, 0x8A); /* 74h - FDh (1) */
	*(u16*)(fw_buffer + 0x1FF72) = swiCRC16(0xFFFF, fw_buffer+0x1FF00, 0x70); /* 00h - 6Fh (2) */
	*(u16*)(fw_buffer + 0x1FFFE) = swiCRC16(0xFFFF, fw_buffer+0x1FF74, 0x8A); /* 74h - FDh (2) */
	dumpBufferToFile("settingsPatchedMemory.bin", fw_buffer);
	
	programming();
	firmware_read(0, fw_buffer, FW_SIZE);
	dumpBufferToFile("settingsProgrammed.bin", fw_buffer);
	if (verifying() < 0)
	{
		return EXIT_FAILURE;
	}

	iprintf("  ** DONE! ENJOY FAKEWAY! **\n");
#endif
	iprintf("  >> PRESS (A) TO EXIT\n");
	WAIT_FOR_BUTTON_PRESS(KEY_A);
	return 0;
}

