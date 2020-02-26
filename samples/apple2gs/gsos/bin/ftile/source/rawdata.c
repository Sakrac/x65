

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "rawdata.h"


RAWDATA* loadRaw(const char* path)
{
	FILE* rawFile = fopen( path, "rb" );

	if (rawFile)
	{
		RAWDATA* pData = (RAWDATA*)malloc(sizeof(RAWDATA));

		fseek(rawFile, 0, SEEK_END);
		pData->size = ftell(rawFile);

		fseek(rawFile, 0, SEEK_SET);

		pData->data = (unsigned char*)malloc(pData->size);

		size_t read_size = fread(pData->data, 1, pData->size, rawFile);

		if (read_size != pData->size)
		{
			printf("WARNING: read %ld of %ld bytes\n", read_size, pData->size);
		}

		fclose(rawFile);

		return pData;
	}

	return NULL;
}

void saveRaw(RAWDATA* pData, const char* path)
{
	FILE* rawFile = fopen( path, "wb" );

	if (rawFile)
	{
		fwrite(pData->data, 1, pData->size, rawFile);
		fclose(rawFile);
	}
}

// rawdata.c

