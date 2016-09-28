#include "net_include.h"
#include "message_defs.h"
#include "sendto_dbg.h"

#define NAME_LENGTH 80

int sock; // Socket
struct ackMessage msg; // Latest message from receiver
struct sockaddr_in rcv_addr;

struct dataMessage window[WINDOW_SIZE];
int window_start = 0; // Index in `window` corresponding to earliest sequence

FILE *fr; // File to be read
unsigned int last_seq = -1; // Number of last sequence in file

// Args
int loss_rate_percent;
char* src_file_name;
char* dest_file_name;
char* rcv_name;
char verbose = 0;

int  CreateSocket();
void InitializeRcv();
void ParseArguments();

void Send(struct dataMessage *);
void ReadAndSendWindowSeq(int index);
int Receive();
int Select();

int main(int argc, char **argv)
{
    sock = CreateSocket();
    ParseArguments(argc, argv);
    InitializeRcv();

    sendto_dbg_init(loss_rate_percent);

    // Open file
    if((fr = fopen(src_file_name, "r")) == NULL) {
      perror("fopen");
      exit(0);
    }

    if (verbose) printf("Successfully opened file %s\n", src_file_name);

    // Send connect message
    struct dataMessage connectMsg;
    connectMsg.seqNo = -1;
    connectMsg.numBytes = strlen(dest_file_name) + 1;
    memcpy(connectMsg.data, dest_file_name, strlen(dest_file_name) + 1);
    Send(&connectMsg);

    if (verbose) printf("Sent connect msg. Dest file name: %s\n", connectMsg.data);


    // Confirm connection with receiver
    char waiting = 0;
    while( 1 ) {    
      int fd_num = Select();

      // If timeout, resend connect message
      if (fd_num == 0 && !waiting) {
	Send(&connectMsg);
	continue;
      }

      // Receive, ignoring messages from other IPs
      if ( !Receive() ) continue;
 
      if (msg.cAck == -2) {
	// Receiver busy
	continue;
      }
      else if (msg.cAck == -1) {
	// Receiver ready
	break;
      }
      else {
	perror("Ncp: Unrecognized cAck value\n");
	exit(1);
      }
    }

    if (verbose) printf("Connection established\n");

    // Send first window
    for (int i = 0; i < WINDOW_SIZE; i++) {
      window[i].seqNo = i;
      ReadAndSendWindowSeq(i);
    }

    if (verbose) printf("Sent first window. last_seq = %d\n", (int) last_seq);
    
    
    // Send remainder of file
    while(1) {
      int fd_num = Select();

      // If timeout, continue waiting
      if (fd_num == 0) {
	if (verbose) printf("Timeout. base = %d, last_seq = %d\n", window[window_start].seqNo - 1, (int) last_seq);
	continue;
      }
	
      // Receive message, ignoring messages from other IPs
      if ( !Receive() ) continue;

      if (verbose) printf("cAck: %d\n", msg.cAck);

      // Discard messages that are "behind" the current window state
      if (window[window_start].seqNo != 0 && msg.cAck < window[window_start].seqNo - 1) continue;

      
      // Shift window past cAck. Send new sequences that appear
      // at the end of the window.
      if (msg.cAck != -1) {
	while(window[window_start].seqNo <= msg.cAck) {
	  window_start = (window_start + 1) % WINDOW_SIZE;
	
	  int index = (window_start + (WINDOW_SIZE - 1)) % WINDOW_SIZE;
	  window[index].seqNo = window[window_start].seqNo + WINDOW_SIZE - 1;
	  ReadAndSendWindowSeq(index);
	}
      }

      // If receiving cAck for the final sequence, break.
      if (msg.cAck != -1 && msg.cAck == last_seq) break;

      // Resend nAcked packets
      for (int i = 0; i < WINDOW_SIZE; i++) {
	int index = (window_start + i) % WINDOW_SIZE;
	if (msg.nAcks[i]) {
	  Send(&window[index]);
	}
      }
    }


    // Close connection
    struct dataMessage disconnectMsg;
    disconnectMsg.seqNo = -2;
    Send(&disconnectMsg);

    if (verbose) printf("Sent disconnect message. last_seq = %d\n", (int) last_seq);

    // Confirm connection closed
    while(1) {
      int fd_num = Select();

      // If timeout, resend disconnect message
      if (fd_num == 0) {
	Send(&disconnectMsg);
	if (verbose) printf("Resent disconnect message. last_seq = %d\n", (int) last_seq);
	continue;
      }

      // Receive, ignoring messages from other IPs
      if ( !Receive() ) continue;

      // Break upon receiving disconnect ack
      if (msg.cAck == -3) break;

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

void Send(struct dataMessage * seq) {
  sendto_dbg( sock, (char*) seq, sizeof(struct dataMessage), 0,
	  (struct sockaddr *)&rcv_addr, sizeof(struct sockaddr_in));
}

int Receive() {
  socklen_t from_len = sizeof(struct sockaddr_in);
  struct sockaddr_in temp_addr;
  recvfrom( sock, (char*) &msg, sizeof(struct ackMessage), 0,
	    (struct sockaddr *)&temp_addr, &from_len);

  // Discard messages from other IPs
  if( temp_addr.sin_addr.s_addr == rcv_addr.sin_addr.s_addr )
    return 1;
  else
    return 0;
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

void ReadAndSendWindowSeq(int index) {
  window[index].numBytes = fread(window[index].data, 1, SEQ_SIZE, fr);

  // Send sequence. Mark end of file if the fread() call
  // returned less than SEQ_SIZE bytes
  if (window[index].numBytes > 0) {
    Send(&window[index]);
    if (window[index].numBytes < SEQ_SIZE) {
      last_seq = window[index].seqNo;
    }
  }
  else if (last_seq == -1) {
    // Mark previous sequence as last if current sequence is 0 bytes
    last_seq = window[index].seqNo - 1;
  }
}

void InitializeRcv() {
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

  rcv_addr.sin_family = AF_INET;
  rcv_addr.sin_addr.s_addr = host_num;
  rcv_addr.sin_port = htons(PORT);
}

void ParseArguments(int argc, char** argv) {
  // Parse arguments
  if (argc < 4) {
    printf("Ncp: Wrong number of arguments");
    exit(1);
  }
  loss_rate_percent = strtol(argv[1], NULL, 10);
  src_file_name = argv[2];
  dest_file_name = strtok(argv[3], "@");
  rcv_name = strtok(NULL, "@");

  if (argc > 4 && ( !strcmp(argv[4], "-v") || !strcmp(argv[4], "v" ) ) ) {
      verbose = 1;
  }
}
