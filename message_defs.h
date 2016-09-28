#define WINDOW_SIZE 1024
#define SEQ_SIZE 1024
#define TIMEOUT_SEC 0
#define TIMEOUT_USEC 2000



struct dataMessage {
    unsigned int seqNo;
    unsigned int numBytes;
    unsigned char data[SEQ_SIZE];
};

struct ackMessage {
    unsigned int cAck;
    unsigned char nAcks[WINDOW_SIZE];
};

