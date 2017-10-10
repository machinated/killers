#ifndef _MESSAGE
#define _MESSAGE
#include <stdint.h>

typedef enum Tag
{
    TAG_ENQUEUE,
    TAG_CANCEL,
    TAG_REQUEST,
    TAG_ACK,
    TAG_TAKE,   // ?
    TAG_RELEASE,
    TAG_REVIEW
} Tag;

#define ACK_OK 0
#define ACK_REJECT 1

typedef union Message {
    struct data_t {
        uint64_t clk;
        uint8_t data[24];
    } data;
    uint8_t buffer[sizeof(struct data_t)];
} Message;

typedef struct MessageEnqueue {
    uint8_t companiesMask[20];  // NYI XXX
} MessageEnqueue;

typedef struct MessageCancel {
    int company;
} MessageCancel;

typedef struct MessageRequest {
    int company;
    int killer;
} MessageRequest;

typedef struct MessageTake {
    int company;
    int killer;
} MessageTake;

typedef struct MessageAck {
    int ack;
} MessageAck;

typedef struct MessageRelease {
    int company;
    int killer;
} MessageRelease;

typedef struct MessageReview {
    int company;
    float review;
} MessageReview;

#endif
