#include "net_include.h"
#include "message_defs.h"

#define NAME_LENGTH 80
#define TIMEOUT_SEC 10

int sock;
char mess_buf[MAX_MESS_LEN];
struct sockaddr_in send_addr;
struct dataMessage connectMsg;
struct dataMessage disconnectMsg;

struct dataMessage window[WINDOW_SIZE];
int window_start = 0;
unsigned char window_sent[WINDOW_SIZE] = { '\0' };

int loss_rate_percent = strtol(argv[1], NULL, 10);
char* src_file_name;
char* dest_file_name;
char* rcv_name;

int  CreateSocket();

int main(int argc, char **argv)
{
    sock = CreateSocket();

    // Parse arguments
    if (argc != 4) {
      printf("Ncp: Wrong number of arguments");
      exit(1);
    }
    loss_rate_percent = strtol(argv[1], NULL, 10);
    src_file_name = argv[2];
    dest_file_name = strtok(argv[3], (const char*) '@');
    rcv_name = strtok(NULL, (const char*) '@');
    
    // Resolve receiver IP address
    struct hostent h_ent; 
    struct hostent *p_h_ent = gethostbyname(rcv_name);
    if ( p_h_ent == NULL ) {
        printf("Ncp: gethostbyname error.\n");
        exit(1);
    }

    int host_num;
    memcpy( &h_ent, p_h_ent, sizeof(h_ent));
    memcpy( &host_num, h_ent.h_addr_list[0], sizeof(host_num) );

    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = host_num;
    send_addr.sin_port = htons(PORT);


    // Create connect message
    connectMsg.seqNo = -1;
    connectMsg.numBytes = strlen(dest_file_name) + 1;

    // Copy dest_file_name string, including \0 at end.
    memcpy(connectMsg.data, dest_file_name, strlen(dest_file_name) + 1);
    printf("Connect message data field is %s\n", connectMsg.data);

    fd_set mask, dummy_mask, temp_mask;
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    FD_ZERO( &mask );
    FD_ZERO( &dummy_mask );
    FD_SET( sock, &mask );

    // Send connect message
    sendto( sock, &connectMsg, sizeof(struct dataMessage), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));
    
    // Confirm connection with receiver
    int waiting = 0;
    while( 1 ) {    
        temp_mask = mask;
        int fd_num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
        if (fd_num > 0) {
            recvfrom( sock, mess_buf, sizeof(mess_buf), 0, (struct sockaddr *)&temp_addr, &from_len);
	    struct ackMessage * msg = (struct ackMessage *) mess_buff;

	    if (msg->cAck == -2) {
	      // receiver is busy. wait.
	      continue;
	    } else if (msg->cAck == -1) {
	      // start sending data
	      break;
	    } else {
	      perror("Ncp: Unrecognized cAck value\n");
	      exit(1);
	    }
        } else {
	  if (!waiting) {
	    // On timeout, resend connect message
	    sendto( sock, &connectMsg, sizeof(struct dataMessage), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));
	  }
	}
    }

    
    // Open file
    FILE *fr; //file to be read
    if((fr = fopen(argv[2], "r")) == NULL) {
        perror("fopen");
        exit(0);
    }

    printf("Opened file for reading");


    // Fill and send first window
    unsigned int last_seqNo = -1;
    for (int i = 0; i < WINDOW_SIZE; i++) {
      window[i].seqNo = i;
      window[i].numBytes = fread(window[i].data, 1, CHUNK_SIZE, fr);

      if (window[i].numBytes > 0) {
	sendto( sock, &window[i], sizeof(struct dataMessage), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
	window_sent[i] = 1;
	if (window[i].numBytes < CHUNK_SIZE)  last_seqNo = window[i].seqNo;
      } else {
	last_seqNo = window[i].seqNo - 1;
      }

      if (window[i].numBytes < CHUNK_SIZE) break;
    }
    
    
    // Proceed to send remainder of file
    while(1) {
      temp_mask = mask;
      int fd_num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
      if (fd_num > 0) {
	for (int i = 0; i < WINDOW_SIZE; i++) window_sent[i] = 0;
	socklen_t from_len = sizeof(struct sockaddr_in);
	struct sockaddr_in temp_addr;
	int bytes = recvfrom( sock, mess_buf, sizeof(mess_buf), 0,
			      (struct sockaddr *)&temp_addr, &from_len);
	struct ackMessage* msg = (struct ackMessage*) mess_buf;

	// Discard messages from other IPs
	if( temp_addr.sin_addr.s_addr != send_addr.sin_addr.s_addr ) continue;

	// If received cAck for the last sequence, break.
	if (msg->cAck = last_seqNo) break;

	// Discard messages that are "behind" the current window state
	if (msg->cAck < window[window_start].seqNo) continue;

	// Shift window according to cAck. Read in and send new packets
	while(window[window_start].seqNo <= msg->cAck) {
	  window_start = (window_start + 1) % WINDOW_SIZE;
	  int index = (window_start + (WINDOW_SIZE -1)) % WINDOW_SIZE;
	  window[index].seqNo = window[window_start].seqNo + WINDOW_SIZE - 1;
	  window[index].numBytes = fread(window[i].data, 1, CHUNK_SIZE, fr);

	  if (window[index].numBytes > 0) {
	    sendto( sock, &window[index], sizeof(struct dataMessage), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
	    window_sent[index] = 1;
	    if (window[index].numBytes < CHUNK_SIZE)  last_seqNo = window[index].seqNo;
	  } else {
	    last_seqNo = window[index].seqNo - 1;
	  }

	  if (window[index].numBytes < CHUNK_SIZE) break;
	}

	// Resend nAcked packets
	for (int i = 0; i < WINDOW_SIZE; i++) {
	  int index = (window_start + i) % WINDOW_SIZE;
	  if (msg->nAcks[i]) {
	    sendto( sock, &window[index], sizeof(struct dataMessage), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
	    window_sent[index] = 1;
	  }
	}
      } else {
	// On timeout, esend previously sent packets
	for (int i = 0; i < WINDOW_SIZE; i++) {
	  if (window_sent[i]) {
	    sendto( sock, &window[i], sizeof(struct dataMessage), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
	  }
	}
      }
    } // End while


    // Close connection
    disconnectMsg.seqNo = -2;
    while(1) {
      // Ask to disconnect
      sendto( sock, &disconnectMsg, sizeof(struct dataMessage), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));
      
      temp_mask = mask;
      int fd_num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
      if (fd_num > 0) {
	socklen_t from_len = sizeof(struct sockaddr_in);
	struct sockaddr_in temp_addr;
	int bytes = recvfrom( sock, mess_buf, sizeof(mess_buf), 0,
			      (struct sockaddr *)&temp_addr, &from_len);
	struct ackMessage* msg = (struct ackMessage*) mess_buf;

	// Discard messages from other IPs
	if( temp_addr.sin_addr.s_addr != send_addr.sin_addr.s_addr ) continue;

	// Break upon receiving disconnect ack
	if (msg->cAck == -3) break;
      }

      // Loop back around on timeout
      
    } // End while


    // Cleanup
    fclose(fr);
    printf("Done!\n");
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
