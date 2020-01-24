
#pragma once
#include <cstdlib>

typedef unsigned long crc_hash;
typedef unsigned long crc_size;
typedef void* crc_buffer;

crc_buffer crc_allocate(crc_size size);
void crc_deallocate(crc_buffer buffer);

// crc hash algorithm from Github: https://github.com/Zer0Mem0ry/CRC32
unsigned int crc_crypt(crc_buffer pData, crc_size iLen);
