#include "net_include.h"
#include "message_defs.h"
#include "sendto_dbg.h"

#define NAME_LENGTH 80
#define MAX_SENDERS 512

int sock; // Socket
int loss_rate_percent; // Command line arg
struct dataMessage msg; // Latest message from sender
struct ackMessage ackMsg; // Latest message from us (receiver)
char ip_str[16]; // Destination for IntToIP() result
FILE *fw = NULL; // Destination file

// Queue of senders.
struct sender { struct sockaddr_in addr; char file[NAME_LENGTH]; };
struct sender senders[MAX_SENDERS];
int front_index = 0;
int back_index = 0;

// Window
struct dataMessage window[WINDOW_SIZE];
int window_start = 0;
int window_base = 0;

void ResetWindow();
void ShiftWindow();
char WindowFull();

int CreateSocket();
char* IntToIP(int ip);
void ParseArguments();

void SendToCurrent(struct ackMessage *);
void Send(struct ackMessage *, struct sockaddr_in *);
struct sockaddr_in Receive();
int Select();

int AddToQueue(struct sender *sender);
int PopFromQueue();
struct sender * QueueFront();

int main(int argc, char **argv)
{
    sock = CreateSocket();
	ResetWindow();
	ParseArguments(argc, argv);
	
	sendto_dbg_init(loss_rate_percent);
    
    while(1) {
      int fd_num = Select();

	  // If timeout, send cAcks and nAcks for missing messages
	  if (fd_num == 0) {
		if (QueueFront() != NULL) {
		  ShiftWindow();
		}
		continue;
	  }

	  // Receive message into `msg`
	  struct sockaddr_in temp_addr = Receive();

	  // Received disconnect message
	  if (msg.seqNo == -2) {
		
		// Dequeue the current sender, if there is one
		if (QueueFront() != NULL && temp_addr.sin_addr.s_addr == (QueueFront()->addr).sin_addr.s_addr) {		  
		  printf("Disconnecting from sender %s (file %s)\n", IntToIP(temp_addr.sin_addr.s_addr), QueueFront()->file);
			  
		  PopFromQueue();
		  fclose(fw);
		  ResetWindow();
			  
		  if (QueueFront() != NULL) {
			printf("Now serving queued sender %s. Writing to file %s\n", IntToIP((QueueFront()->addr).sin_addr.s_addr), QueueFront()->file);

			// Open file
			if((fw = fopen(QueueFront()->file, "w")) == NULL) {
			  perror("fopen");
			  exit(0);
			}

			// Send ack
			ackMsg.cAck = -1;
			SendToCurrent(&ackMsg);
		  }
		}

		// Send response regardless of whether the disconnect message is from the current sender
		struct ackMessage disconnectMsg;
		disconnectMsg.cAck = -3;
		Send(&disconnectMsg, &temp_addr);
		continue;
	  }


	  // Serve sender of msg if we're not serving anyone
	  if (QueueFront() == NULL) {
		struct sender sndr;
		sndr.addr = temp_addr;
		memcpy(sndr.file, msg.data, strlen(msg.data) + 1);
		AddToQueue(&sndr);

		printf("Now serving sender %s. Writing to file %s\n", IntToIP(temp_addr.sin_addr.s_addr), msg.data);
	  }

	  // If we're busy serving another sender
	  if (temp_addr.sin_addr.s_addr != QueueFront()->addr.sin_addr.s_addr) {
		// Queue this sender (we'll serve them when we're not busy)
		struct sender sndr;
		sndr.addr = temp_addr;
		memcpy(sndr.file, msg.data, strlen(msg.data) + 1);
		AddToQueue(&sndr);
			
		// send BUSY message
		struct ackMessage busyMsg;
		busyMsg.cAck = -2;
		Send(&busyMsg, &temp_addr);

		printf("Queuing sender %s (file %s)\n", IntToIP(temp_addr.sin_addr.s_addr), msg.data);
		continue;
	  }

	  if (msg.seqNo == -1) {
		// Receiving connect message
			  
		// Open file
		if((fw = fopen(msg.data, "w")) == NULL) {
		  perror("fopen");
		  exit(0);
		}

		// Send ack
		ackMsg.cAck = -1;
		SendToCurrent(&ackMsg);
		continue;
	  }

	  
	  // Receiving data message
			  
	  // Write to corresponding window slot (provided the sequence number is within the window)
	  if (msg.seqNo >= window_base && msg.seqNo < window_base + WINDOW_SIZE) {
		int index = (window_start + (msg.seqNo - window_base)) % WINDOW_SIZE;
		window[index] = msg;
	  }

	  // If this message completes the current window, let the sender know with a cAck.
	  // Otherwise, wait for the rest of the messages.
	  if (WindowFull()) {
		ShiftWindow();
	  }
	  	  
	} // End while
}

int PopFromQueue() {
    front_index = (front_index + 1) % MAX_SENDERS;
    return 1;
}

int AddToQueue(struct sender *sender){
    //check if sender is already in the queue

    for (int i = front_index; i < back_index; i = (i + 1) % MAX_SENDERS ) {
      if (senders[i].addr.sin_addr.s_addr == sender->addr.sin_addr.s_addr) {
	    return 0;
	  }
    }
    senders[back_index] = *sender;
    back_index = (back_index + 1) % MAX_SENDERS;
    return 1;
}

struct sender* QueueFront() {
    return front_index == back_index ? NULL : &(senders[front_index]);
}


int CreateSocket() {
  struct sockaddr_in name;
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    perror("Ncp: socket");
    exit(1);
  }

  name.sin_family = AF_INET;
  name.sin_addr.s_addr = INADDR_ANY;
  name.sin_port = htons(PORT);

  if ( bind( s, (struct sockaddr*)&name, sizeof(name) ) < 0 ) {
    perror("Ncp: bind");
    exit(1);
  }

  return s;
}

char* IntToIP(int ip) {
  sprintf(ip_str, "%d.%d.%d.%d",
		  (htonl(ip) & 0xff000000)>>24,
		  (htonl(ip) & 0x00ff0000)>>16,
		  (htonl(ip) & 0x0000ff00)>>8,
		  (htonl(ip) & 0x000000ff));
  return ip_str;
}

char WindowFull() {
  for (int i = 0; i < WINDOW_SIZE; i++) {
	if (window[i].numBytes == 0)
	  return 0;
  }

  return 1;
}

void ResetWindow() {
  for (int i = 0; i < WINDOW_SIZE; i++) {
	window[i].numBytes = 0;
  }
  window_start = 0;
  window_base = 0;
}

void ShiftWindow() {
  // Slide window as far as necessary, writing to disk any packets that we slide past
  int num_shifted = 0;
  while (window[window_start].numBytes != 0) {
	int nwritten = fwrite(window[window_start].data, 1, window[window_start].numBytes, fw);
	if (nwritten < window[window_start].numBytes) {
	  perror("nwritten < numBytes\n");
	  exit(0);
	}

	window_base++; 
	window[window_start].numBytes = 0;
	window_start = (window_start + 1) % WINDOW_SIZE;
	num_shifted++;

	if ( (window_base - 1) * SEQ_SIZE % (1024 * 1024 * 100) == 0 ) { 
	  printf("Cumulative Mbytes transferred: %d.  Rate (Mbits/sec): %d\n", (window_base - 1) * SEQ_SIZE / (1024 * 1024), -1);
	}
  }

  // Send cAcks and nAcks based on window state
  ackMsg.cAck = window_base - 1;
  for (int i = 0; i < WINDOW_SIZE - num_shifted; i++) {
	ackMsg.nAcks[i] = window[(window_start + i) % WINDOW_SIZE].numBytes == 0 ? 1 : 0;
  }
  for (int i = WINDOW_SIZE - num_shifted; i < WINDOW_SIZE; i++) {
	ackMsg.nAcks[i] = 0;
  }
  SendToCurrent(&ackMsg);
}


void SendToCurrent(struct ackMessage * ack) {
  sendto_dbg( sock, (char*) ack, sizeof(struct ackMessage), 0,
		  (struct sockaddr *)&(QueueFront()->addr), sizeof(struct sockaddr_in));
}

void Send(struct ackMessage * ack, struct sockaddr_in * addr) {
  sendto_dbg( sock, (char*) ack, sizeof(struct ackMessage), 0,
		  (struct sockaddr *)addr, sizeof(struct sockaddr_in));
}

struct sockaddr_in Receive() {
  socklen_t from_len = sizeof(struct sockaddr_in);
  struct sockaddr_in temp_addr;
  recvfrom( sock, (char*) &msg, sizeof(struct dataMessage), 0,
	    (struct sockaddr *)&temp_addr, &from_len);

  return temp_addr;
}

int Select() {
  fd_set mask, dummy_mask, temp_mask;
  struct timeval timeout;
  
  FD_ZERO( &mask );
  FD_ZERO( &dummy_mask );
  FD_SET( sock, &mask );

  temp_mask = mask;
  timeout.tv_sec = TIMEOUT_SEC;
  timeout.tv_usec = TIMEOUT_USEC;
	
  return select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
}

void ParseArguments(int argc, char** argv) {
  // Parse arguments
  if (argc != 2) {
    printf("Ncp: Wrong number of arguments");
    exit(1);
  }
  loss_rate_percent = strtol(argv[1], NULL, 10);
}
