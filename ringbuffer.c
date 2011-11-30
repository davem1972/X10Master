/*
 * (C) Copyright 2011, Dave McCaldon <davem@mccaldon.com>
 * All Rights Reserved.
 *
 * Ring buffer implementation for general use.
 * 
 */

#include <stdint.h>
#include <stddef.h>

#if !defined(__ringbuffer_h__)
#include "ringbuffer.h"
#endif

/**
 * Initialize a ring buffer with the specified buffer
 */
void RB_Initialize(RB_RINGBUFFER* rb, uint8_t* buffer, size_t size)
{
    rb->buffer = buffer;
    rb->size   = size;
    rb->head   = 0;
    rb->tail   = 0;
}

/**
 * Insert into the ring buffer.  We insert data at the head pointer until it
 * meets the tail.  Note that since head==tail means buffer empty, we can't
 * completely fill the ring buffer.  We must also wrap the buffer at the end
 * as needed.
 */
size_t RB_Insert(RB_RINGBUFFER* rb, uint8_t* data, size_t length)
{
    size_t ptr = 0;

    while (ptr < length) {
        // Calculate space remaining
        size_t space = RB_Free(rb);

        // Check to see if there is any room at all
        if (!space) break;

        // Copy in our data at the head pointer
        while ((space-- > 0) && (ptr < length)) {
            rb->buffer[rb->head++] = data[ptr++];
        }

        // Wrap the head pointer if we hit the end of the buffer
        if (rb->head == rb->size) rb->head = 0;

        // Keep looping until either all of the data has been inserted,
        // or we've filled the buffer.
    }

    // Return how many bytes we were able to copy into the buffer

    return ptr;
}

size_t __RB_Reader(RB_RINGBUFFER* rb, uint8_t* data, size_t count, void** token)
{
    uint8_t* out = (uint8_t*)(*token);
    size_t   ptr = 0;

    while (ptr < count) {
        *(out++) = data[ptr++];
    }
    
    (*token) = (void*)out;


    return ptr;
}

/**
 * Read from the ring buffer, returns the number of bytes retrieved
 */
size_t RB_Read(RB_RINGBUFFER* rb, uint8_t* out, size_t max)
{
    void* token = (void*)out;

    return RB_ReadWithCallback(rb, max, __RB_Reader, &token);
}

/**
 * Callback based ringbuffer reader
 */
size_t RB_ReadWithCallback(RB_RINGBUFFER* rb, size_t max, RB_PFNREADCALLBACK rcb, void** token)
{
    size_t sent = 0;
    size_t data = 0;

    // Loop while there is data available in the ring buffer
    while (((data = RB_DataAvailable(rb)) > 0) && (sent < max)) {

        // Only send up to the max specified
        if ((max - sent) < data) data = (max - sent);

        // Now, invoke the callback to "read" the data
        while (data > 0) {
            // The callback will return how much it read
            size_t used = rcb(rb, &rb->buffer[rb->tail], data, token);

            // Stop if the callback didn't read anything ...
            if (!used) break;

            // Account for what was read
            data -= used;
            sent += used;

            // Update the tail pointer accordingly
            rb->tail += used;

            if (rb->tail >= rb->size) {
                // Wrap the tail if we hit the end of the buffer
                rb->tail = 0;
            }
        }
    }

    // Return what we consumed from the buffer overall

    return sent;
}

/**
 * Reset a ring buffer to empty
 */
void RB_Reset(RB_RINGBUFFER* rb)
{
    rb->head = 0;
    rb->tail = 0;
}

/**
 * Calculate data available in the ring buffer.  Note that there may actually
 * be more data available than this indicates, since it doesn't take into
 * account space beyond the "wrap" of the end of the buffer.
 */
size_t RB_DataAvailable(RB_RINGBUFFER* rb)
{
    /* Since the tail follows the head, if the tail is less than the head, we
       can retrieve the data in between.  Otherwise, the head ptr has wrapped
       and we can copy the data between the tail and the end of the buffer.

       If head==tail, then the buffer is empty.
    */
    return (rb->head >= rb->tail ? rb->head - rb->tail : rb->size - rb->tail);
}

/**
 * Calculate free space in the ring buffer.  Note that there may actually
 * be more space available than this indicates, since it doesn't take into
 * account space beyond the "wrap" of the end of the buffer.
 */
size_t RB_Free(RB_RINGBUFFER* rb)
{
    /* If the head is bigger than the tail, then we can simply use the buffer
       up to the end.  Otherwise, if the buffer has wrapped and head is less
       than the tail, we can use the space up to the tail, but we must stay
       back one byte so head != tail (which means no data in the buffer)
    */
    int free = (int)rb->tail - (int)rb->head;

    if (free > 0) {
        free--;
    } else {
        free = (int)rb->size - (int)rb->head;
        if (!rb->tail) free--;
    }

    return (size_t)free;
}

/*
 * End-of-file
 *
 */
