/**************************************************************************
 * simpletun.c                                                            *
 *                                                                        *
 * A simplistic, simple-minded, naive tunnelling program using tun/tap    *
 * interfaces and TCP. DO NOT USE THIS PROGRAM FOR SERIOUS PURPOSES.      *
 *                                                                        *
 * You have been warned.                                                  *
 *                                                                        *
 * (C) 2010 Davide Brini.                                                 *
 *                                                                        *
 * DISCLAIMER AND WARNING: this is all work in progress. The code is      *
 * ugly, the algorithms are naive, error checking and input validation    *
 * are very basic, and of course there can be bugs. If that's not enough, *
 * the program has not been thoroughly tested, so it might even fail at   *
 * the few simple things it should be supposed to do right.               *
 * Needless to say, I take no responsibility whatsoever for what the      *
 * program might do. The program has been written mostly for learning     *
 * purposes, and can be used in the hope that is useful, but everything   *
 * is to be taken "as is" and without any kind of warranty, implicit or   *
 * explicit. See the file LICENSE for further details.                    *
 *************************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>
#include "buffer.h"
#include <pthread.h>
#include <time.h>
#include<errno.h>

/* buffer for reading from tun/tap interface, must be >= 1500 */
#define BUFSIZE 65000 
#define COMPSIZE BUFSIZE + 64
#define CLIENT 0
#define SERVER 1
#define PORT 55555

//1/2 second delay/interval
#define DELAY_NSEC 500000000
#include "minicomp/minicomp.h"
int debug;
char *progname;

pthread_mutex_t lock;

struct sender_args_struct {
    int net_fd;
    node_t ** payload_buffer;

};
/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. The caller     *
 *            must reserve enough space in *dev.                          *
 **************************************************************************/
int tun_alloc(char *dev, int flags) {

    struct ifreq ifr;
    int fd, err;
    char *clonedev = "/dev/net/tun";

    if( (fd = open(clonedev , O_RDWR)) < 0 ) {
        perror("Opening /dev/net/tun");
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = flags;

    if (*dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    if( (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
        perror("ioctl(TUNSETIFF)");
        close(fd);
        return err;
    }

    strcpy(dev, ifr.ifr_name);

    return fd;
}

/**************************************************************************
 * cread: read routine that checks for errors and exits if an error is    *
 *        returned.                                                       *
 **************************************************************************/
int cread(int fd, char *buf, int n){

    int nread;

    if((nread=read(fd, buf, n)) < 0){
        perror("Reading data");
        exit(1);
    }
    return nread;
}

/**************************************************************************
 * cwrite: write routine that checks for errors and exits if an error is  *
 *         returned.                                                      *
 **************************************************************************/
int cwrite(int fd, char *buf, int n){

    int nwrite;

    /* printf("In cwrite, fd is: %d, buf must be right, n is: %d\n", fd, n); */

    if((nwrite=write(fd, buf, n)) < 0){
        /* printf("Problems w/ writing%s\n", strerror(errno)); */
        /* fflush(stdout); */
        perror("Writing data");
        exit(1);
    }
    return nwrite;
}

/**************************************************************************
 * read_n: ensures we read exactly n bytes, and puts them into "buf".     *
 *         (unless EOF, of course)                                        *
 **************************************************************************/
int read_n(int fd, char *buf, int n) {

    int nread, left = n;

    while(left > 0) {
        if ((nread = cread(fd, buf, left)) == 0){
            return 0 ;      
        }else {
            left -= nread;
            buf += nread;
        }
    }
    return n;  
}

/**************************************************************************
 * do_debug: prints debugging stuff (doh!)                                *
 **************************************************************************/
void do_debug(char *msg, ...){

    va_list argp;

    if(debug) {
        va_start(argp, msg);
        vfprintf(stderr, msg, argp);
        va_end(argp);
    }
}

/**************************************************************************
 * my_err: prints custom error messages on stderr.                        *
 **************************************************************************/
void my_err(char *msg, ...) {

    va_list argp;

    va_start(argp, msg);
    vfprintf(stderr, msg, argp);
    va_end(argp);
}

/**************************************************************************
 * usage: prints usage and exits.                                         *
 **************************************************************************/
void usage(void) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "%s -i <ifacename> [-s|-c <serverIP>] [-p <port>] [-u|-a] [-d]\n", progname);
    fprintf(stderr, "%s -h\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "-i <ifacename>: Name of interface to use (mandatory)\n");
    fprintf(stderr, "-s|-c <serverIP>: run in server mode (-s), or specify server address (-c <serverIP>) (mandatory)\n");
    fprintf(stderr, "-p <port>: port to listen on (if run in server mode) or to connect to (in client mode), default 55555\n");
    fprintf(stderr, "-u|-a: use TUN (-u, default) or TAP (-a)\n");
    fprintf(stderr, "-d: outputs debug information while running\n");
    fprintf(stderr, "-h: prints this help text\n");
    exit(1);
}

int sender(void* args_ptr) {
    struct sender_args_struct * args  = (struct sender_args_struct * ) args_ptr;

    //define the delay
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = DELAY_NSEC;
    

    //pointer to first node in the payload buffer
    //TODO: locks/semaphores
    //TODO: after freeing the current struct, we will have to update payload_buffer pointer
    //to point to new head


    node_t **payload_buffer = args->payload_buffer;
    char comp_buffer[COMPSIZE];
    uint16_t nwrite, plength;
    int net_fd = args->net_fd;

    //initialize the padding buffer
    char zero_buffer[COMPSIZE];
    for (int i = 0; i < COMPSIZE; i++) {
        zero_buffer[i] = '0';
    }
    plength = htons(0);
    memcpy(zero_buffer, (char*) &plength, sizeof(plength));

    while(1) {
        //LOCK THE LIST
        pthread_mutex_lock(&lock);

        node_t * node = *payload_buffer;
        size_t byte_total = node->len;
        size_t num_packets = node->num_packets;
        char* packet_buffer = node->buffer;

        //do_debug ("in while loop, byte total is: %d\n", byte_total);

        //TODO: add delay feature
        //there is a packet, so we send
        if (byte_total > 2) {
            //compress into comp_buffer, leaving enough space for prepending size
            
            do_debug("Buffer of length%d: %.*s\n", byte_total, byte_total, packet_buffer);
            size_t comp_size = minicomp(comp_buffer+sizeof(plength), packet_buffer, byte_total, COMPSIZE);
            
            memcpy((char*) & plength, packet_buffer, sizeof(plength));
            size_t num_pack_test = ntohs(plength);

            if (comp_size > COMPSIZE) { //sanity check
                do_debug("ERROR, TOO MANY COMPRESSED BYTES, This should never be reached!!!\n");
                exit(1);
            }

            //prepend the size of compressed data to the packet
            //needed for successful decompression
            plength = htons(comp_size);
            memcpy(comp_buffer, (char*) &plength, sizeof(plength));

            //copy zeroes into buffer:
            //TODO: uncomment this code to test after threding works
            //am i off by one in number of bytes or anything? 
            //memcpy(comp_buffer+offset, zero_buffer, COMPSIZE-offset);
            size_t offset = comp_size + sizeof(plength);
            for (int i = offset; i < COMPSIZE; i++) {
                //fill buffer, for now with zeros
                //TODO: we can optimize this using memcpy
                comp_buffer[i] = '0';
            }

            nwrite = cwrite(net_fd, comp_buffer, COMPSIZE);

            /* do_debug("wrote buffer: %s\n", buffer); */
            /* do_debug("TAP2NET %lu: Written %d bytes to the network\n", tap2net, nwrite); */
            //reset the buffers
            byte_total = sizeof(plength);
            comp_size = 0;
            num_packets = 0;

            if (node->next) {
                *payload_buffer = node->next;
            }
            else {
                    //create new node
                    node_t *new_node = malloc(sizeof(node_t));
                    new_node->buffer = malloc(sizeof(char) * BUFSIZE + 1);
                    new_node->len = byte_total;
                    new_node->num_packets = 0;
                    new_node->next = NULL;
                    *payload_buffer = new_node;
            }
            //TODO: free the node
        }
        else {
            //write the padding bytes
            nwrite = cwrite(net_fd, zero_buffer, COMPSIZE);
            //todo: send a buffer with all zeros (if compsize is zero we will just ignore the packet)
        }

        pthread_mutex_unlock(&lock);
        nanosleep(&delay, NULL);
    }
}


int main(int argc, char *argv[]) {

    int tap_fd, option;
    int flags = IFF_TUN;
    char if_name[IFNAMSIZ] = "";
    int maxfd;
    uint16_t nread, nwrite, plength;
    char buffer[BUFSIZE];
    char comp_buffer[COMPSIZE];
    struct sockaddr_in local, remote;
    char remote_ip[16] = "";            /* dotted quad IP string */
    unsigned short int port = PORT;
    int sock_fd, net_fd, optval = 1;
    socklen_t remotelen;
    int cliserv = -1;    /* must be specified on cmd line */
    unsigned long int tap2net = 0, net2tap = 0;

    progname = argv[0];

    /* Check command line options */
    while((option = getopt(argc, argv, "i:sc:p:uahd")) > 0) {
        switch(option) {
            case 'd':
                debug = 1;
                break;
            case 'h':
                usage();
                break;
            case 'i':
                strncpy(if_name,optarg, IFNAMSIZ-1);
                break;
            case 's':
                cliserv = SERVER;
                break;
            case 'c':
                cliserv = CLIENT;
                strncpy(remote_ip,optarg,15);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'u':
                flags = IFF_TUN;
                break;
            case 'a':
                flags = IFF_TAP;
                break;
            default:
                my_err("Unknown option %c\n", option);
                usage();
        }
    }

    argv += optind;
    argc -= optind;

    if(argc > 0) {
        my_err("Too many options!\n");
        usage();
    }

    if(*if_name == '\0') {
        my_err("Must specify interface name!\n");
        usage();
    } else if(cliserv < 0) {
        my_err("Must specify client or server mode!\n");
        usage();
    } else if((cliserv == CLIENT)&&(*remote_ip == '\0')) {
        my_err("Must specify server address!\n");
        usage();
    }

    /* initialize tun/tap interface */
    if ( (tap_fd = tun_alloc(if_name, flags | IFF_NO_PI)) < 0 ) {
        my_err("Error connecting to tun/tap interface %s!\n", if_name);
        exit(1);
    }

    do_debug("Successfully connected to interface, verifying with a write%s\n", if_name);

    if ( (sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(1);
    }

    if(cliserv == CLIENT) {
        /* Client, try to connect to server */

        /* assign the destination address */
        memset(&remote, 0, sizeof(remote));
        remote.sin_family = AF_INET;
        remote.sin_addr.s_addr = inet_addr(remote_ip);
        remote.sin_port = htons(port);

        /* connection request */
        if (connect(sock_fd, (struct sockaddr*) &remote, sizeof(remote)) < 0) {
            perror("connect()");
            exit(1);
        }

        net_fd = sock_fd;
        do_debug("CLIENT: Connected to server %s\n", inet_ntoa(remote.sin_addr));

    } else {
        /* Server, wait for connections */

        /* avoid EADDRINUSE error on bind() */
        if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0) {
            perror("setsockopt()");
            exit(1);
        }

        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(port);
        if (bind(sock_fd, (struct sockaddr*) &local, sizeof(local)) < 0) {
            perror("bind()");
            exit(1);
        }

        if (listen(sock_fd, 5) < 0) {
            perror("listen()");
            exit(1);
        }

        /* wait for connection request */
        remotelen = sizeof(remote);
        memset(&remote, 0, remotelen);
        if ((net_fd = accept(sock_fd, (struct sockaddr*)&remote, &remotelen)) < 0) {
            perror("accept()");
            exit(1);
        }

        do_debug("SERVER: Client connected from %s\n", inet_ntoa(remote.sin_addr));
    }


    /* use select() to handle two descriptors at once */
    maxfd = (tap_fd > net_fd)?tap_fd:net_fd;
    //node_t * packet_buffer = NULL;
    /* char packet_buffer[BUFSIZE]; */
    char *packet_buffer;

    size_t packet_buffer_len = 0;
    size_t byte_total = sizeof(plength);//number of packets indicator at beginnign
    size_t num_packets = 0;

    //we want a pointer, so we can change the address of first node
    //TODO: dynamically allocate this node, so we can free it like all the others
    node_t * payload_buffer;
    node_t node; 
    payload_buffer = &node;

    node.len = byte_total;
    node.next = NULL;
    node.buffer = malloc(sizeof(char) * BUFSIZE + 1);
    node.num_packets = 0;

    struct sender_args_struct args;
    args.net_fd = net_fd;
    args.payload_buffer = &payload_buffer;


    pthread_t sender_thread;
    if(pthread_create(&sender_thread, NULL, (void*) &sender, (void*) &args)) {
        perror("pthread_create failed, exiting\n");
        exit(1);
    }

    while(1) {


        int ret;
        fd_set rd_set;

        FD_ZERO(&rd_set);
        FD_SET(tap_fd, &rd_set); FD_SET(net_fd, &rd_set);

        ret = select(maxfd + 1, &rd_set, NULL, NULL, NULL);

        if (ret < 0 && errno == EINTR){
            continue;
        }

        if (ret < 0) {
            perror("select()");
            exit(1);
        }

        if(FD_ISSET(tap_fd, &rd_set)) {
            printf("\n\n****WRITING TO NETWORK***\n\n");
            /* data from tun/tap: just read it and write it to the network */

            //find the node that we will append to (the last one)

            //LOCK THE LIST
            pthread_mutex_lock(&lock);

            node_t * last_node = payload_buffer;
            while (last_node->next) {
                last_node = last_node->next;
            }
            packet_buffer = last_node->buffer;
            byte_total = last_node->len;
            num_packets = last_node->num_packets;
            do_debug("Sanity: last_node->num_packets was: %lu\n", num_packets);

            //TODO: need to actually initialize the values for last node somewhere
            nread = cread(tap_fd, buffer, BUFSIZE);

            tap2net++;
            do_debug("TAP2NET %lu: Read %d bytes from the tap interface\n", tap2net, nread);

            /***check if packet will fit in current node, else allocate a new node for it***/
            //if it's greater than MTU, we need to verify that it compresses to <MTU

            //TODO: make sure the size of buffer is >COMPSIZE, so we can compress it down
            int check_compsize = 0;
            if (byte_total + nread + sizeof(plength) > BUFSIZE) {
                check_compsize = 1;
            }

            /***add the packet to current node***/
            num_packets++;
            //put number of packets at beginning of buffer
            plength = htons(num_packets); //first element is number of packets included in buffer
            memcpy(packet_buffer, (char* ) & plength, sizeof(plength));
            //we dont' increment byte_total after copying num_packets because we pre-allocated the constant sizeof(plength) above

            //size of this packet
            plength = htons(nread);
            memcpy(packet_buffer + byte_total, (char*) & plength, sizeof(plength));
            byte_total += sizeof(plength);
            //append the packet's buffer
            memcpy(packet_buffer + byte_total, buffer, nread);
            byte_total += nread;

            //TODO: buffer/destlen should be bufsize+64, not compsize, so we can get a true size
            if(check_compsize) {
                //if it's too big for the buffer
                if (minicomp(comp_buffer, packet_buffer, byte_total, COMPSIZE) > COMPSIZE) {

                    //ignore the "last packet" that was written into buffer
                    num_packets--;
                    plength = htons(num_packets); 
                    memcpy(packet_buffer, (char* ) & plength, sizeof(plength));

                    num_packets = 1;
                    byte_total = sizeof(plength);//only 2 bytes so far, for num_packets

                    //create new node
                    node_t *new_node = malloc(sizeof(node_t));
                    new_node->buffer = malloc(sizeof(char) * BUFSIZE + 1);
                    new_node->num_packets = num_packets;
                    new_node->len = byte_total;

                    //update packet_buffer poiner
                    packet_buffer = new_node->buffer;

                    //write the new stuff into the buffer
                    plength = htons(num_packets); 
                    memcpy(packet_buffer, (char* ) & plength, sizeof(plength));

                    plength = htons(nread);
                    memcpy(packet_buffer + byte_total, (char*) & plength, sizeof(plength));
                    byte_total += sizeof(plength);
                    //append the packet's buffer
                    memcpy(packet_buffer + byte_total, buffer, nread);
                    byte_total += nread;


                    new_node->len = byte_total;
                    last_node->next = new_node;
                }
            }


            last_node->len = byte_total;
            last_node->num_packets = num_packets;
            /***end adding packet to node***/

            pthread_mutex_unlock(&lock);


        }

        //we don't need to modify this, if we read from the network we can handle it internally ASAP
        if(FD_ISSET(net_fd, &rd_set)) {
            /* data from the network: read it, and write it to the tun/tap interface. 
             * We need to read the length first, and then the packet */

            /* Read length */      
            /* nread = read_n(net_fd, (char *)&plength, sizeof(plength)); */
            //if(nread == 0) {
            //    /* ctrl-c at the other end */
            //    break;
            //}

            //net2tap++;

            /* read packet */
            //nread = read_n(net_fd, buffer, ntohs(plength));
            nread = read_n(net_fd, comp_buffer, COMPSIZE);
            if (nread == 0) {
                //    /* ctrl-c at the other end */
                break;
            }

            memcpy((char*) & plength, comp_buffer, sizeof(plength));
            size_t comp_size = ntohs(plength);
            if (comp_size) {
                do_debug("\n\n***READ VALID DATA FROM NETWORK*****\n\n");
                do_debug("Read a compsize of: %d\n", comp_size);

                //decompress into buffer
                nread = minidecomp(buffer, comp_buffer+sizeof(plength), comp_size, BUFSIZE);
                /* memcpy(buffer, comp_buffer, comp_size); */
                /* do_debug("NET2TAP %lu: Read %d bytes from the network\n", net2tap, nread); */

                memcpy((char*) & plength, buffer, sizeof(plength));
                size_t num_packets = ntohs(plength);
                do_debug("num_packets: %lu, bytes: %lu\n", num_packets, nread);

                size_t offset = sizeof(plength); //starting offset is just offset by initial packet number
                char tmp_buf[BUFSIZE];
                for (int packet = 0; packet < num_packets; packet++) {
                    //get the size of this packet
                    memcpy((char*) &plength, buffer+offset, sizeof(plength));
                    offset += sizeof(plength);
                    //get the buffer for this packet
                    memcpy(tmp_buf, buffer+offset, ntohs(plength));
                    offset += ntohs(plength);

                    do_debug("Buffer of length%d: %.*s\n", ntohs(plength), ntohs(plength), tmp_buf);
                    nwrite = cwrite(tap_fd, tmp_buf, ntohs(plength));
                    do_debug("NET2TAP %lu: Written %d bytes to the tap interface\n", net2tap, nwrite);
                }
            }
            //we read a padding packet
            else {
                do_debug("Read a padding packet!\n");
            }
        }


        //so now we need to unwind the packet
        //first is the number of packets




        /* now buffer[] contains a full packet or frame, write it into the tun/tap interface */ 
        //no need to buffer the packet, because it is incoming so there is no need
        //nwrite = cwrite(tap_fd, buffer, nread);
        //do_debug("NET2TAP %lu: Written %d bytes to the tap interface\n", net2tap, nwrite);
    }

    return(0);
}
