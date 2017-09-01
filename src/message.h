#ifndef _MESSAGE
#define _MESSAGE
#include <stdint.h>

#define TAG_ACK 1
//#define TAG_OFFER 2
#define TAG_REQUEST 3
#define TAG_UPDATE 4
#define TAG_CANCEL 5
#define TAG_REVIEW 6

//const char* tag_names[7] = {"0", "ACK", "2", "REQUEST", "UPDATE", "CANCEL", "REVIEW"};

#define ACK_OK 0
#define ACK_REJECT 1

typedef union Message {
    struct data_t {
        //int pid;
        uint64_t clk;
        uint8_t data[24];
    } data;
    uint8_t buffer[sizeof(struct data_t)];
} Message;

typedef struct MessageUpdate {
    int queueIndex;
} MessageUpdate;

typedef struct MessageAck {
    int ack;
} MessageAck;

typedef struct MessageReview {
    int company;
    float review;
} MessageReview;

#endif
