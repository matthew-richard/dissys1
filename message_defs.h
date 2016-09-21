#define WINDOW_SIZE 512
#define CHUNK_SIZE 1024


struct dataMessage {
    unsigned int seqNo;
    unsigned int numBytes;
    unsigned char data[CHUNK_SIZE];
};

struct ackMessage {
	unsigned int cAck;
	unsigned int nAcks[WINDOW_SIZE];
};

