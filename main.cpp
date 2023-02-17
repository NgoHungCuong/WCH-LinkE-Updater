#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint32_t nhc_usb_open(void);
void nhc_usb_close(void);
uint32_t nhc_usb_write(uint8_t u8Ep, uint8_t* pu8Data, uint32_t u32Len);
uint32_t nhc_usb_read(uint8_t u8Ep, uint8_t* pu8Data, uint32_t u32Len);

#ifdef _WIN32

#pragma warning(disable : 4996)

#include <windows.h>
#include "CH375DLL.H"
#pragma comment(lib, "CH375DLL.lib")

uint32_t nhc_usb_open(void)
{

	if (CH375OpenDevice(0) == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	return 1;
}

void nhc_usb_close(void)
{

	CH375CloseDevice(0);
}

uint32_t nhc_usb_write(uint8_t u8Ep, uint8_t* pu8Data, uint32_t u32Len)
{
	uint32_t u32Tmp;

	u32Tmp = u32Len;
	if (CH375WriteEndP(0, u8Ep, pu8Data, (PULONG) &u32Tmp))
	{
		if (u32Tmp == u32Len)
		{
			return 1;
		}
	}

	return 0;
}

uint32_t nhc_usb_read(uint8_t u8Ep, uint8_t* pu8Data, uint32_t u32Len)
{
	uint32_t u32Tmp;

	u32Tmp = u32Len;
	if (CH375ReadEndP(0, u8Ep & ~(0x80), pu8Data, (PULONG) &u32Tmp))
	{
		if (u32Tmp == u32Len)
		{
			return 1;
		}
	}

	return 0;
}

#else

#include <string.h>
#include "libusb-1.0/libusb.h"
libusb_device_handle* h;

uint32_t nhc_usb_open(void)
{
	
	libusb_init(NULL);
	h = libusb_open_device_with_vid_pid(NULL, 0x4348, 0x55e0);

	return h != NULL;
}

void nhc_usb_close(void)
{

	libusb_close(h);
	libusb_exit(NULL);
}

uint32_t nhc_usb_write(uint8_t u8Ep, uint8_t* pu8Data, uint32_t u32Len)
{
	uint32_t u32Tmp;

	u32Tmp = u32Len;
	if (libusb_bulk_transfer(h, u8Ep, pu8Data, u32Len, (int*) &u32Tmp, 5000) != 0)
	{
		return 0;
	}

	return 1;
}

uint32_t nhc_usb_read(uint8_t u8Ep, uint8_t* pu8Data, uint32_t u32Len)
{
	uint32_t u32Tmp;

	u32Tmp = u32Len;
	if (libusb_bulk_transfer(h, u8Ep | 0x80, pu8Data, u32Len, (int*) &u32Tmp, 5000) != 0)
	{
		return 0;
	}

	return 1;
}

#endif

int main(int argc, char** argv)
{

	printf("WCH-LinkE-Updater by Ngo Hung Cuong\n");
	if (argc != 2)
	{
		printf("usage: WCH-LinkE-Updater flash-file.bin\n");
		return 0;
	}
	
	if (nhc_usb_open() == 0)
	{
		printf("Found no WCH-LinkE\n");
		return 0;
	}

	FILE* f;
	uint8_t* pu8Data;

	f = fopen(argv[1], "rb");

	if (!f)
	{
		printf("Open flash file: FAIL\n");
		return 0;
	}

	uint32_t u32FileSize;

	fseek(f, 0, SEEK_END);

	u32FileSize = ftell(f);

	if (u32FileSize > (128 * 1024 - 8 * 1024))
	{
		printf("Wrong flash file\n");
		return 0;
	}

	fseek(f, 0, SEEK_SET);
	pu8Data = (uint8_t *)malloc(u32FileSize);
	fread(pu8Data, 1, u32FileSize, f);
	fclose(f);

	uint8_t u8RxBuff[64];
	uint8_t u8TxBuff[64];

	/* Set address */
	u8TxBuff[0] = 0x81;
	u8TxBuff[1] = 0x02;
	u8TxBuff[2] = 0x00;
	u8TxBuff[3] = 0x00;

	if (!nhc_usb_write(0x02, u8TxBuff, 4))
	{
		printf("Send data: FAIL\n");
		return 0;
	}

	if (!nhc_usb_read(0x82, u8RxBuff, 2))
	{
		printf("Receive data: FAIL\n");
		return 0;
	}

	/* check result */
	if (u8RxBuff[0] || u8RxBuff[1])
	{
		printf("Check data: FAIL\n");
		return 0;
	}

	uint32_t n;
	uint32_t i;

	/* write data */
	printf("Write:\n");
	n = u32FileSize / 0x3c;
	for (i = 0; i < n; ++i)
	{
		u8TxBuff[0] = 0x80;
		u8TxBuff[1] = 0x3c;
		u8TxBuff[2] = (i * 0x3c);
		u8TxBuff[3] = (i * 0x3c / 256);
		memmove(u8TxBuff + 4, pu8Data + i * 0x3c, 0x3c);
		
		if (!nhc_usb_write(0x02, u8TxBuff, 0x40))
		{
			printf("Send data: FAIL\n");
			return 0;
		}

		if (!nhc_usb_read(0x82, u8RxBuff, 2))
		{
			printf("Receive data: FAIL\n");
			return 0;
		}

		/* check result */
		if (u8RxBuff[0] || u8RxBuff[1])
		{
			printf("Check data: FAIL\n");
			return 0;
		}
		printf(".");
		fflush(stdout);
	}
	if (u32FileSize % 0x3c)
	{
		u8TxBuff[0] = 0x80;
		u8TxBuff[1] = u32FileSize % 0x3c;
		u8TxBuff[2] = (i * 0x3c);
		u8TxBuff[3] = (i * 0x3c / 256);
		memmove(u8TxBuff + 4, pu8Data + i * 0x3c, u32FileSize % 0x3c);

		if (!nhc_usb_write(0x02, u8TxBuff, u32FileSize % 0x3c + 0x04))
		{
			printf("Send data: FAIL\n");
			return 0;
		}

		if (!nhc_usb_read(0x82, u8RxBuff, 2))
		{
			printf("Receive data: FAIL\n");
			return 0;
		}

		/* check result */
		if (u8RxBuff[0] || u8RxBuff[1])
		{
			printf("Check data: FAIL\n");
			return 0;
		}
		printf(".");
	}

	printf("\n");
	fflush(stdout);

	/* Set address */
	
#if 0
	u8TxBuff[0] = 0x81;
	u8TxBuff[1] = 0x02;
	u8TxBuff[2] = 0x00;
	u8TxBuff[3] = 0x00;

	if (!nhc_usb_write(0x02, u8TxBuff, 4))
	{
		printf("Send data: FAIL\n");
		return 0;
	}

	if (!nhc_usb_read(0x82, u8RxBuff, 2))
	{
		printf("Receive data: FAIL\n");
		return 0;
	}

	/* check result */
	if (u8RxBuff[0] || u8RxBuff[1])
	{
		printf("Check data: FAIL\n");
		return 0;
	}
#endif

	/* verify data */
	printf("Verify:\n");
	n = u32FileSize / 0x3c;
	for (i = 0; i < n; ++i)
	{
		u8TxBuff[0] = 0x82;
		u8TxBuff[1] = 0x3c;
		u8TxBuff[2] = (i * 0x3c);
		u8TxBuff[3] = (i * 0x3c / 256);
		memmove(u8TxBuff + 4, pu8Data + i * 0x3c, 0x3c);

		if (!nhc_usb_write(0x02, u8TxBuff, 0x40))
		{
			printf("Send data: FAIL\n");
			return 0;
		}

		if (!nhc_usb_read(0x82, u8RxBuff, 2))
		{
			printf("Receive data: FAIL\n");
			return 0;
		}

		/* check result */
		if (u8RxBuff[0] || u8RxBuff[1])
		{
			printf("Check data: FAIL\n");
			return 0;
		}
		printf(".");
		fflush(stdout);
	}
	if (u32FileSize % 0x3c)
	{
		u8TxBuff[0] = 0x82;
		u8TxBuff[1] = u32FileSize % 0x3c;
		u8TxBuff[2] = (i * 0x3c);
		u8TxBuff[3] = (i * 0x3c / 256);
		memmove(u8TxBuff + 4, pu8Data + i * 0x3c, u32FileSize % 0x3c);

		if (!nhc_usb_write(0x02, u8TxBuff, u32FileSize % 0x3c + 0x04))
		{
			printf("Send data: FAIL\n");
			return 0;
		}

		if (!nhc_usb_read(0x82, u8RxBuff, 2))
		{
			printf("Receive data: FAIL\n");
			return 0;
		}

		/* check result */
		if (u8RxBuff[0] || u8RxBuff[1])
		{
			printf("Check data: FAIL\n");
			return 0;
		}
		printf(".");
	}

	printf("\n");
	fflush(stdout);

	/* reset and run */
	u8TxBuff[0] = 0x83;
	u8TxBuff[1] = 0x02;
	u8TxBuff[2] = 0x00;
	u8TxBuff[3] = 0x00;

	nhc_usb_write(0x02, u8TxBuff, 4);

	printf("OK\n");

	return 0;
}
