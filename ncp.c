#include "net_include.h"
#include "message_defs.h"

#define NAME_LENGTH 80
#define TIMEOUT_SEC 10

int  CreateSocket();

int main(int argc, char **argv)
{
    int sock = CreateSocket();
    struct sockaddr_in send_addr;
    struct dataMessage dataMsg;
    struct dataMessage connectMsg;


    // Parse arguments
    if (argc != 4) {
      printf("Ncp: Wrong number of arguments");
      exit(1);
    }
    int loss_rate_percent = strtol(argv[1], NULL, 10);
    char* src_file_name = argv[2];
    const char* at = "@";
    char* dest_file_name = strtok(argv[3], at);
    char* comp_name = strtok(NULL, at);
    
    // Resolve receiver IP address
    struct hostent h_ent; 
    struct hostent *p_h_ent = gethostbyname(comp_name);
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

    // Start sending data

    
    

    

    FILE *fr; //file to be read
    char buf[CHUNK_SIZE]; //buffer for file copy
    int nread, nwritten;

    if((fr = fopen(argv[2], "r")) == NULL) {
        perror("fopen");
        exit(0);
    }

    printf("Opened file for reading");


    // start loop on window
    nread = fread(buf, 1 , CHUNK_SIZE, fr); //read in a chunk of the file

    if(nread > 0) {
        memcpy(dataMsg.data, buf, nread);
    }

    dataMsg.seqNo = 0;
    dataMsg.numBytes = nread;
    printf("DataMsg numBytes is: %d\n ", dataMsg.numBytes);
    
    sendto( sock, &dataMsg, sizeof(struct dataMessage), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));

    // end loop
    


    // Cleanup
    fclose(fr);


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
