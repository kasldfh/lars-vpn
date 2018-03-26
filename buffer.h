#ifndef BUFFER
#define BUFFER
//I will be using a simple linked list as the queue at first. 
/* New messages will be added to the end of the list, and messages to be sent will
   be removed from the beginning of the list. This will ensure that the sender has
   constant time access to the buffer it needs to send. It is acceptable for there 
   to be a small delay ( O(n) ) adding to end of the buffer because the sender will
   be sending the whole time
   */

#define BUFFER_LEN	4096

//nanosleep needs delay in nanoseconds
#define DELAY_MICROSEC 1000
#define DELAY_NANOSEC DELAY_MICROSEC * 1000

//the packet size (minus headers) that we will pad to / # of bytes read from urandom
#define PACKET_MAX 420

typedef struct node {
    size_t len;
    char* buffer;
    struct node * next;
} node_t;


//struct is two pointers and a size_t
#define NODE_SIZE sizeof(void*) * 2 + sizeof(size_t)


#endif
