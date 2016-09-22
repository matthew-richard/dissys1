#include "net_include.h"
#include "message_defs.h"

#define NAME_LENGTH 80
#define TIMEOUT_SEC 10
#define MAX_SENDERS 512
#define WINDOW_SIZE 512

int sock;
fd_set mask, dummy_mask, temp_mask;
char mess_buf[MAX_MESS_LEN];
struct timeval timeout;
FILE *fw = NULL;

// Queue of senders. 
struct sockaddr_in senders[MAX_SENDERS];
int front_index = 0;
int back_index = 0;

int CreateSocket();

int AddToQueue(struct sockaddr_in *sender);
int PopFromQueue();
struct sockaddr_in* QueueFront();

int main(int argc, char **argv)
{
    sock = CreateSocket();

    //file copy boiler plate code
    int nwritten, nread;

    

    FD_ZERO( &mask );
    FD_ZERO( &dummy_mask );
    FD_SET( sock, &mask);
    for(;;)
    {
      temp_mask = mask;
      timeout.tv_sec = TIMEOUT_SEC;
      timeout.tv_usec = 0;

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
			AddToQueue(&temp_addr);
		  }

		  if (temp_addr.sin_addr.s_addr == QueueFront()->sin_addr.s_addr) {
			printf("IPs equal\n");
			// Do processing
			if (msg->seqNo == -1) {
			  printf("Seq no: %d\n", msg->seqNo);
			  
			  // Open file
			  if((fw = fopen(msg->data, "w")) == NULL) {
				perror("fopen");
				exit(0);
			  }

			  printf("Opened file %s\n", msg->data);

			  // Send ack
			  struct ackMessage ackMsg;
			  ackMsg.cAck = 0;
			  sendto( sock, &ackMsg, sizeof(struct ackMessage), 0, (struct sockaddr *)QueueFront(), sizeof(*QueueFront()));
			  printf("Sent ACK message\n");
		  
			}
		  } else {
			// 
			AddToQueue(&temp_addr);
			// send BUSY message
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
		// Resend previous message...?
		printf("Timed out!\n");
	  }
    }
  }
int PopFromQueue() {
    front_index = (front_index + 1)%MAX_SENDERS;
    return 1;
}

int AddToQueue(struct sockaddr_in *sender){
    int i;
    for (i = front_index; i < back_index; i = (i + 1) % MAX_SENDERS ) {
        if (senders[i].sin_addr.s_addr == sender->sin_addr.s_addr) {
            //check if sender is already in the queue
            return 0;
        }
    }
    senders[back_index] = *sender;
    back_index = (back_index + 1)%MAX_SENDERS;
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
