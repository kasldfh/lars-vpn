#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <time.h>
#include <bsd/stdlib.h>

#include "buffer.h"

//TODO: maybe this shouldn't be "global" variable
//initialize an "empty" list
node_t * packet_buffer = NULL;
node_t * payload_buffer = NULL;

//mutexes for buffers
pthread_mutex_t packet_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t payload_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

/*this function is designed to send out a packet on some fixed interval
  the sending interval is defined in SVPN_SEND_INTERVAL (in milliseconds)
  it will send entries in the "svpn_send_buffer", which should be compressed but not encrypted
  if there is no data to be sent, it will send a buffer of MTU length read from dev/urandom
*/
//int svpn_sender(void* svpn_addresses) {
//
//    struct svpn_server* plele = svpn_addresses;
//
//
//    struct timespec delay;
//    delay.tv_sec = 0;
//    delay.tv_nsec = DELAY_NANOSEC;
//
//    struct svpn_client *psc = svpn_addresses;
//
//    char* buf;
//    int len;
//
//    while(1) {
//        if(packet_buffer) {//if the buffer is empty this will be set to NULL
//            node_t * node = packet_buffer;
//            //thread safe list operations
//            pthread_mutex_lock(&packet_buffer_mutex);
//            packet_buffer = node->next; 
//            pthread_mutex_unlock(&packet_buffer_mutex);
//
//            buf = node->buffer;
//            len = node->len;
//
//            //free the old head of the list
//            free(node);
//        }
//        else {
//            //TODO: read some packets from /dev/urandom, then encrypt, then send
//            buf = malloc(PACKET_MAX);
//            arc4random_buf(buf, PACKET_MAX);
//            len = PACKET_MAX; 
//        }
//
//        int len = sendto(psc->sock_fd, buf, len, 0,
//                (struct sockaddr*)&(psc->server_addr), sizeof(psc->server_addr));
//
//        //free le buffer
//        free(buf);
//
//        //second argument is the remaining time if thread wakes up early (if some handler is #triggered)
//        //should never happen
//        nanosleep(&delay, NULL);
//    }
//
//    return 0;
//}

