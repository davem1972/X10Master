/*
 * (C) Copyright 2011, Dave McCaldon <davem@mccaldon.com>
 * All Rights Reserved.
 *
 * Ring buffer implementation for general use.
 * 
 */

#if !defined(__ringbuffer_h__)
#define __ringbuffer_h__

/**
 * Ring buffer structure definition.
 */
typedef struct {
    uint8_t*        buffer;
    size_t          size;
    volatile size_t head;
    volatile size_t tail;
} RB_RINGBUFFER;

/**
 * Callback typedef
 */
typedef size_t (*RB_PFNREADCALLBACK)(RB_RINGBUFFER*,uint8_t*,size_t,void**);

/**
 * Initialize a ring buffer with the specified buffer
 */
void RB_Initialize(RB_RINGBUFFER* rb, uint8_t* buffer, size_t size);

/**
 * Insert into the ring buffer
 */
size_t RB_Insert(RB_RINGBUFFER* rb, uint8_t* data, size_t length);

/**
 * Read from the ring buffer, returns the number of bytes retrieved
 */
size_t RB_Read(RB_RINGBUFFER* rb, uint8_t* out, size_t max);

/**
 * Callback based ringbuffer reader
 */
size_t RB_ReadWithCallback(RB_RINGBUFFER* rb, size_t max, RB_PFNREADCALLBACK cb, void** token);

/**
 * Reset a ring buffer to empty
 */
void RB_Reset(RB_RINGBUFFER* rb);

/**
 * Calculate data available
 */
size_t RB_DataAvailable(RB_RINGBUFFER* rb);

/**
 * Calculate free space
 */
size_t RB_Free(RB_RINGBUFFER* rb);

#endif // __ringbuffer_h__

/*
 * End-of-file
 *
 */
