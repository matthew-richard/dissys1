#include "net_include.h"
#include "message_defs.h"

#define NAME_LENGTH 80
#define TIMEOUT_SEC 10

int CreateSocket();

int main(int argc, char **argv)
{
    char host_name[NAME_LENGTH] = {'\0'};
    char my_name[NAME_LENGTH] = {'\0'};
    int sock;
    struct sockaddr_in from_addr;
    fd_set mask, dummy_mask, temp_mask;
    char mess_buf[MAX_MESS_LEN];
    struct timeval timeout;

    sock = CreateSocket();

    //file copy boiler plate code
    FILE *fw;
    char buf[CHUNK_SIZE];
    int nwritten;

    char* new_file = "new_file.txt";
    if((fw = fopen(new_file, "w")) == NULL) {
        perror("fopen");
        exit(0);
    }
    

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
	  socklen_t from_len = sizeof(from_addr);
	  int bytes = recvfrom( sock, mess_buf, sizeof(mess_buf), 0,
				(struct sockaddr *)&from_addr, &from_len);
	  //printf( "Received %d bytes\n", bytes );
	  //printf( "1: %d, 2: %d, 3: %d, 4: %d", mess_buf[0], mess_buf[1], mess_buf[2], mess_buf[3] );
	  mess_buf[bytes] = 0;
	  int from_ip = from_addr.sin_addr.s_addr;

	  /*unsigned int sn = 0;
	  memcpy(&sn, mess_buf, sizeof(int));

	  printf( "sn: %d", sn);*/

	  struct dataMessage* msg = (struct dataMessage*) mess_buf;

	  
      printf( "Sequence number: %d\n", (*msg).seqNo );
      printf("Data %s\n", msg->data );

	  /*printf( "Received from (%d.%d.%d.%d): %s\n", 
                                (htonl(from_ip) & 0xff000000)>>24,
                                (htonl(from_ip) & 0x00ff0000)>>16,
                                (htonl(from_ip) & 0x0000ff00)>>8,
                                (htonl(from_ip) & 0x000000ff),
                                mess_buf );*/
	}
      }
    }
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
