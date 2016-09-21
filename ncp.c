#include "net_include.h"
#include "message_defs.h"

#define NAME_LENGTH 80
#define TIMEOUT_SEC 10

void PromptForHostName( char *my_name, char *host_name, size_t max_len );
int  CreateSocket();

int main(int argc, char **argv)
{
    char my_name[NAME_LENGTH] = {'\0'};
    int sock;
    struct sockaddr_in send_addr;
    struct dataMessage dataMsg;
    dataMsg.seqNo = 1024;
    char* str = "Yo Ron sup.";

    if (argc != 4) {
      printf("Ncp: Wrong number of arguments");
      exit(1);
    }

    int loss_rate_percent = strtol(argv[1], NULL, 10);
    char* src_file_name = argv[2];
    const char* at = "@";
    char* dest_file_name = strtok(argv[3], at);
    char* comp_name = strtok(NULL, at);

    printf("%i", loss_rate_percent);
    printf(src_file_name);
    printf(dest_file_name);
    printf(comp_name);

    sock = CreateSocket();
    //PromptForHostName(my_name, host_name, NAME_LENGTH);

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
    
    sendto( sock, &dataMsg, sizeof(struct dataMessage), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));
    //sendto( sock, str, strlen(str), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));

}

void PromptForHostName( char *my_name, char *host_name, size_t max_len ) {

    char *c;

    gethostname(my_name, max_len );
    printf("My host name is %s.\n", my_name);

    printf( "\nEnter host to send to:\n" );
    if ( fgets(host_name,max_len,stdin) == NULL ) {
        perror("Ucast: read_name");
        exit(1);
    }
    
    c = strchr(host_name,'\n');
    if ( c ) *c = '\0';
    c = strchr(host_name,'\r');
    if ( c ) *c = '\0';

    printf( "Sending from %s to %s.\n", my_name, host_name );

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
