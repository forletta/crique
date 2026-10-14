# Crique

A fixed size circular byte buffer with queue-able variable-size chunks, written
in C.

When writing to a crique, internally each chunk begins with a `size_t` header
that records the length of it's chunk. Writing and enqueueing are separate
operations, so multiple incremental writes per push are supported. After a
chunk is pushed, it can be peeked and popped when it reaches the front of the
queue. 

Crique guarantees individual chunks are contiguous, while chunk-to-chunk may
not be. This contiguous guarantee means, if the back of the crique runs out of
capacity, the bytes from the *current write* are copied to the front if there's
room.

## Basic Usage

Simply include `crique.h` and compile with `crique.c`. The basic usage of a
crique is as follows (for more trivial functions see `crique.h`):

```C
crique_t cq;
uint8_t buf[BUFFER_SIZE];

crique(&cq, buf, sizeof(buf)); // Crique requires initialization.
 
size_t packet_size;
uint8_t *packet = foo(&packet_size);

// `crique_get_write_ptr` returns a pointer to current write position, NOT the
// write chunk. Returns `NULL` if the Crique is full.
uint8_t *write = crique_get_write_ptr(&cq);

if (write == NULL)
    return; // Crique is full.
 
// `crique_write_cap` returns how many bytes can be written to the pointer 
// returned by `crique_get_write_ptr`.
if (packet_size > crique_write_cap(&cq))
    return; // Not enough room in the current write chunk.
 
// ALTERNATIVELY:
// If the current write chunk has enough additional capacity
// `crique_get_write_ptr_ensure_cap` will return the current write pointer.
// If the front chunk has enough capacity, it will move any bytes already 
// written to back chunk to the front, and return the new write pointer.
// Otherwise returns `NULL`.
write = crique_get_write_ptr_ensure_cap(&cq, packet_size);

if (write == NULL)
    return; // Not enough room in the entire crique.
 
memcpy(write, packet, packet_size);

// `crique_written` updates the the current write chunk length. This NEEDS to
// be called any time bytes are written to the write pointer, otherwise
// subsequent writes will overwrite previous ones.
crique_written(&cq, packet_size);

// `crique_push` pushes all the bytes that have been written to the current 
// write chunk, and initializes the next write chunk.
crique_push(&cq);

// Use `crique_peek` and `crique_peek_len` to read the front of the queued
// chunks. Note: once `crique_pop` has been called the bytes previously
// returned by `crique_peek` may be overwritten.
while ((packet = crique_peek(&cq)) != NULL) {
    bar(packet, crique_peek_len(&cq));

    crique_pop(&cq);
}
```
 
## Rationale

This data structure came about when adding asynchronous
[COBS](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing) packet
support to my [USART serial library](https://github.com/forletta/libserial). I
knew I wanted to have a queue of variable sized packets, and it would be nice
if the packets were contiguous to make deserializing easier for the user.
Another requirement was incremental writing. Because the DMA controller
writes to a ping-pong buffer, any given packet may be incomplete if/when it
fills one half of that buffer. To prevent data loss, the potentially
incomplete packet needs to be read from the ping-pong buffer _immediately_.
Since the decoded bytes need to be stored somewhere, being able to write to
the packet queue _without_ needing to push if the packet is incomplete,
seemed only intuitive. It was these three requirements/niceties that
inspired this data structure: queue-able variable-size packets, contiguous
packets, and incremental writing. Crique achieves all three and made
implementing COBS packets for my serial library a breeze.
