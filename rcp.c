#include "net_include.h"
#include "message_defs.h"

#define NAME_LENGTH 80
#define TIMEOUT_SEC 10
#define MAX_SENDERS 512

int sock;
fd_set mask, dummy_mask, temp_mask;
char mess_buf[MAX_MESS_LEN];
struct timeval timeout;
FILE *fw = NULL;

// Previous message sent to current sender
struct ackMessage ackMsg;

// Queue of senders.
struct sender { sockaddr_in addr; char file[NAME_LENGTH}; };
struct sender senders[MAX_SENDERS];
int front_index = 0;
int back_index = 0;

// Window
unsigned char window[WINDOW_SIZE][CHUNK_SIZE];
unsigned char window_received[WINDOW_SIZE] = {'\0'};
int window_base = 0;
int window_start = 0;

int CreateSocket();

int AddToQueue(struct sockaddr_in *sender);
int PopFromQueue();
struct sender * QueueFront();

int main(int argc, char **argv)
{
    sock = CreateSocket();
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;

    //file copy boiler plate code
    int nwritten, nread;

    

    FD_ZERO( &mask );
    FD_ZERO( &dummy_mask );
    FD_SET( sock, &mask);
    for(;;)
    {
      temp_mask = mask;

      int fd_num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout );
      if (fd_num > 0) {
		if ( FD_ISSET( sock, &temp_mask ) ) {
		  socklen_t from_len = sizeof(struct sockaddr_in);
		  struct sockaddr_in temp_addr;
		  int bytes = recvfrom( sock, mess_buf, sizeof(mess_buf), 0,
								(struct sockaddr *)&temp_addr, &from_len);
		  struct dataMessage* msg = (struct dataMessage*) mess_buf;
		  printf("Data: %s\n", msg->data);


		  if (QueueFront() == NULL) {
			printf("Adding to queue\n");
			struct sender sndr;
			sndr.addr = temp_addr;
			memcpy(sndr.file, msg->data, strlen(msg->data) + 1);
			AddToQueue(&sndr);
		  }

		  if (temp_addr.sin_addr.s_addr == QueueFront()->addr.sin_addr.s_addr) {
			printf("IPs equal\n");
			// Do processing
			if (msg->seqNo == -1) {
			  // Connect message
			  
			  //printf("Seq no: %d\n", msg->seqNo);
			  
			  // Open file
			  if((fw = fopen(msg->data, "w")) == NULL) {
				perror("fopen");
				exit(0);
			  }

			  printf("Opened file %s\n", msg->data);

			  // Send ack
			  ackMsg.cAck = -1;
			  sendto( sock, &ackMsg, sizeof(struct ackMessage), 0, (struct sockaddr *) &QueueFront()->addr, sizeof(QueueFront()->addr));
			  printf("Sent ACK message\n");
		  
			} else if (msg->seqNo == -2) {
			  // Disconnect message

			  // Pop from queue
			  // Close file
			  // Reset window state (includes zeroing out window_received)
			  // Open new file for next in queue (if there is one)
			  // Send cAck -1 to next in queue (if there is one)
			} else {
			  // Data message
			  
			  // Write to corresponding window slot (provided the sequence number is within the window)
			  if (msg->seqNo >= window_base && msg->seqNo < window_base + WINDOW_SIZE) {
				int window_index = (window_start + (msg->seqNo - window_base)) % WINDOW_SIZE;
				memcpy(window[window_index], msg->data, CHUNK_SIZE);
				window_received[window_index] = 1;
			  }
			  
			  // Slide window as far as necessary, writing to disk any packets that we slide past
			  while (window_received[window_start] == 1) {
				int nwritten = fwrite(msg->data, 1, msg->numBytes, fw);
				if (nwritten < msg->numBytes) {
				  perror("nwritten < msg->numBytes\n");
				  exit(0);
				}
				
				window_base++;
				window_received[window_start] = 0;
				window_start = (window_start + 1) % WINDOW_SIZE;
			  }

			  // Send cAcks and nAcks based on window state
			  ackMsg.cAck = window_base - 1;
			  for (int i = 0; i < WINDOW_SIZE; i++) {
				ackMsg.nAcks[i] = window_received[(window_start + i) % WINDOW_SIZE] == 0 ? 1 : 0;
			  }
			  sendto( sock, &ackMsg, sizeof(struct ackMessage), 0, (struct sockaddr *) &QueueFront()->addr, sizeof(QueueFront()->addr));
			}
		  } else {
			AddToQueue(&temp_addr);
			
			// send BUSY message
			struct ackMessage busyMsg;
			busyMsg.cAck = -2;
			sendto( sock, &busyMsg, sizeof(struct ackMessage), 0, (struct sockaddr *) &temp_addr, sizeof(temp_addr));
			printf("Sent BUSY message\n");
		  }
	  
	  
		  //mess_buf[bytes] = 0;
		  //int from_ip = from_addr.sin_addr.s_addr;

		  /*unsigned int sn = 0;
			memcpy(&sn, mess_buf, sizeof(int));

			printf( "sn: %d", sn);*/


		  /*nread = msg->numBytes;
		  printf("%d\n", nread);
		  if(nread > 0) {
			nwritten = fwrite(msg->data, 1, nread, fw);
		  }

		  if (nwritten < nread) {
			printf("nwritten<nread\n");
			exit(0);
		  }
		  printf( "Sequence number: %d\n", msg->seqNo );
		  printf("Data %s\n", msg->data );

		  fclose(fw);*/
		  /*printf( "Received from (%d.%d.%d.%d): %s\n", 
			(htonl(from_ip) & 0xff000000)>>24,
			(htonl(from_ip) & 0x00ff0000)>>16,
			(htonl(from_ip) & 0x0000ff00)>>8,
			(htonl(from_ip) & 0x000000ff),
			mess_buf );*/
		}
      } else {
		// On timeout, resend previous message
		sendto( sock, &ackMsg, sizeof(struct ackMessage), 0, (struct sockaddr *) &QueueFront()->addr, sizeof(QueueFront()->addr));
		printf("Timed out!\n");
	  }
    }
}

int PopFromQueue() {
    front_index = (front_index + 1) % MAX_SENDERS;
    return 1;
}

int AddToQueue(struct sockaddr_in *sender){
    //check if sender is already in the queue

    int i;
    for (i = front_index; i < back_index; i = (i + 1) % MAX_SENDERS ) {
      if (senders[i].sin_addr.s_addr == sender->sin_addr.s_addr) {
	    return 0;
	  }
    }
    senders[back_index] = *sender;
    back_index = (back_index + 1) % MAX_SENDERS;
    return 1;
}

struct sockaddr_in* QueueFront() {
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
