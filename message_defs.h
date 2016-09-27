#define WINDOW_SIZE 512
#define SEQ_SIZE 1024


struct dataMessage {
    unsigned int seqNo;
    unsigned int numBytes;
    unsigned char data[SEQ_SIZE];
};

struct ackMessage {
    unsigned int cAck;
    unsigned char nAcks[WINDOW_SIZE];
};

