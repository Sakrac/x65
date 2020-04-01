
#ifndef _rawdata_h_
#define _rawdata_h_

#include "bctypes.h"

typedef struct {
	unsigned char* data; // pointer to data
	size_t size;		 // size in bytes
} RAWDATA;


RAWDATA* loadRaw(const char* path);
void saveRaw(RAWDATA* pData, const char* path);

#endif //_rawdata_h_
