/*
 * (C) Copyright 2011, Dave McCaldon <davem@mccaldon.com>
 * All Rights Reserved.
 *
 * Ring buffer unit tests.
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#if !defined(__ringbuffer_h__)
#include "ringbuffer.h"
#endif

char* text = "QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQqq";

char* text2 = "################################################################################################%%";

char work[1024];

int main(int argc, char** argv)
{
    RB_RINGBUFFER buffer;
    size_t        buffer_size = 512;
    uint8_t       buffer_data[buffer_size];
    size_t        ret;
    off_t         index;
    

    printf("ringbuffer tests ...\n");

    memset((void*)buffer_data, '@', buffer_size);
    memset((void*)work, 0, sizeof(work));

    printf("    Initializing for %zu bytes\n", buffer_size);
    RB_Initialize(&buffer, buffer_data, buffer_size);
    printf("    Buffer has %zu bytes free\n", RB_Free(&buffer));

    printf("    Testing clean insert of %zu bytes\n", strlen(text)-2);
    assert(strlen(text) == 513);
    ret = RB_Insert(&buffer, (uint8_t*)text, strlen(text)-2);
    printf("    RB_Insert() inserted %zu bytes\n", ret);
    printf("    head=%zu,tail=%zu\n", buffer.head, buffer.tail);
    assert(ret == 511);

    for (index = 0; index < ret; index++) assert(buffer_data[index] == text[index]);
    assert(buffer_data[index] == '@');

    printf("    Space free is %zu bytes\n", RB_Free(&buffer));
    printf("    Data available is %zu bytes\n", RB_DataAvailable(&buffer));

    printf("    Testing insert of 1 byte into full buffer\n");
    ret = RB_Insert(&buffer, (uint8_t*)"*", 1);
    assert(ret == 0);

    printf("    Reading data ...\n");
    ret = RB_Read(&buffer, (uint8_t*)work, 256);
    printf("    Read %zu bytes\n", ret);
    printf("    head=%zu,tail=%zu\n", buffer.head, buffer.tail);
    assert(ret == 256);

    for (index = 0; index < ret; index++) assert(work[index] == text[index]);
    assert(work[index] == 0);

    printf("    Space free is %zu bytes\n", RB_Free(&buffer));
    printf("    Data available is %zu bytes\n", RB_DataAvailable(&buffer));

    printf("    Reading 256 more\n");
    ret = RB_Read(&buffer, (uint8_t*)&work[256], 256);
    printf("    Read %zu bytes\n", ret);
    printf("    head=%zu,tail=%zu\n", buffer.head, buffer.tail);
    assert(ret == 255);

    for (index = 0; index < ret; index++) assert(work[index+256] == text[index+256]);
    assert(work[index+256] == 0);

    printf("    Space free is %zu bytes\n", RB_Free(&buffer));
    printf("    Data available is %zu bytes\n", RB_DataAvailable(&buffer));

    printf("    Now inserting 3 bytes\n");
    ret = RB_Insert(&buffer, (uint8_t*)text2, 3);
    printf("    RB_Insert() inserted %zu bytes\n", ret);
    printf("    head=%zu,tail=%zu\n", buffer.head, buffer.tail);
    assert(ret == 3);

    printf("    Space free is %zu bytes\n", RB_Free(&buffer));
    printf("    Data available is %zu bytes\n", RB_DataAvailable(&buffer));

    printf("    Reading 3 more\n");
    ret = RB_Read(&buffer, (uint8_t*)&work[511], 3);
    printf("    Read %zu bytes\n", ret);
    printf("    head=%zu,tail=%zu\n", buffer.head, buffer.tail);
    assert(ret == 3);

    for (index = 0; index < ret; index++) assert(work[index+511] == text2[index]);
    assert(work[index+511] == 0);
    

    return 0;
}

/*
 * End-of-file
 *
 */
