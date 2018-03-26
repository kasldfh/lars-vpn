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

/* buffer for reading from tun/tap interface, must be >= 1500 */
#define BUFSIZE 2000   
#define CLIENT 0
#define SERVER 1
#define PORT 55555

int debug;
char *progname;

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

    if((nwrite=write(fd, buf, n)) < 0){
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

int main(int argc, char *argv[]) {

    int tap_fd, option;
    int flags = IFF_TUN;
    char if_name[IFNAMSIZ] = "";
    int maxfd;
    uint16_t nread, nwrite, plength;
    char buffer[BUFSIZE];
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

    do_debug("Successfully connected to interface %s\n", if_name);

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
    node_t * packet_buffer = NULL;
    size_t packet_buffer_len = 0;

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
            /* data from tun/tap: just read it and write it to the network */

            nread = cread(tap_fd, buffer, BUFSIZE);

            tap2net++;
            do_debug("TAP2NET %lu: Read %d bytes from the tap interface\n", tap2net, nread);

            /* write length + packet */

            //TODO: buffer the packet
            node_t * node = malloc(sizeof(node_t));
            if(!node) {
                perror("mallocing  node failed");
                exit(1);
            }
            node->buffer = malloc(sizeof(char) * nread);

            node->len = nread;
            memcpy(node->buffer, buffer, nread);
            node->next = NULL;

            if (!packet_buffer) {
                packet_buffer = node;
            }
            else {
                node_t * cur = packet_buffer;
                node_t * next = packet_buffer->next;
                while(next) {
                    cur = next;
                    next = cur->next;
                }
                //now next is null, so we append to the list
                cur->next = node;
            }
            packet_buffer_len++;

            //awesome that this is the only place where we have to declare this, we don't have to worry about a "server" and a "client" programs being separate
            //add to packet buffer
            //for the beginning, if packet_buffer size is 4, send the traffic. Otherwise dont send
            //later we will add the threading and all that

            if (packet_buffer_len == 2) {

                 plength = htons(packet_buffer->len);
                 printf("sizeof plength was: %d\n", sizeof(plength));

                do_debug("packet_buffer_len is 2, sending 2 packets from buffer: \n");
                //TODO: determine number of packets to send
                size_t num_packets = 2;
                packet_buffer_len -= num_packets;
                node_t * cur = packet_buffer;

                size_t offset = 0;
                //cast first 
                plength = htons(num_packets); //first element is number of packets included in buffer
                memcpy(buffer + offset, (char* ) & plength, sizeof(plength));
                offset += sizeof(plength);
                do_debug("offset should be 2, is %d\n", offset);

                //sizeof(plength) is the amount of bytes we should offset each thing by. 
                //sizeof(plength) should be a constant (2), but we will put in some checking for it

                //buffer[1] will be len of first packet, etc
                for (int i = 1; i <= num_packets; i++) {
                    plength = htons(cur->len);
                    memcpy(buffer + offset, (char*) &plength, sizeof(plength));
                    offset += sizeof(plength);
                    cur = cur->next;
                }
                cur = packet_buffer;
                //populate the buffer
                for (int i = 1; i <= num_packets; i++) {
                    memcpy(buffer + offset, cur->buffer, cur->len);
                    offset += cur->len;
                    cur = cur->next;
                }

                //at the end of it, cur will equal the new head of packet_buffer (null)
                packet_buffer = cur;
                if (!packet_buffer) {
                    do_debug("packet buffer is null, as expected\n");
                }

                

                //send out the buffer
                //TODO: is there an off by one in the bufsize-offset?
                for (int i = offset; i < BUFSIZE; i++) {
                    //fill buffer, for now with zeros
                    buffer[i] = '0';
                }
                nwrite = cwrite(net_fd, buffer, BUFSIZE);

                do_debug("wrote buffer: %s\n", buffer);
                do_debug("TAP2NET %lu: Written %d bytes to the network\n", tap2net, nwrite);




                    //plength = htons(nread);
                    //plength = htons(packet_buffer->len);
                    //nwrite = cwrite(net_fd, (char *)&plength, sizeof(plength));
                    //nwrite = cwrite(net_fd, buffer, nread);
                    //nwrite = cwrite(net_fd, packet_buffer->buffer, nread);
                    //packet_buffer = packet_buffer->next;
                    //packet_buffer_len--;
            }


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
            nread = read_n(net_fd, buffer, BUFSIZE);
            do_debug("NET2TAP %lu: Read %d bytes from the network\n", net2tap, nread);
            char print_buffer[BUFSIZE+1];
            strncpy(print_buffer, buffer, BUFSIZE);
            do_debug ("NET2TAP, print buffer: %s\n", print_buffer);

            memcpy((char*) & plength, buffer, sizeof(plength));
            size_t num_packets = ntohs(plength);
            do_debug("Read number of packets, it is %d\n", num_packets);

            //the buffer_offset is the location of the next packet buffer. 
            //calculated by calculating space taken by num_packets and each packet's size
            size_t buffer_offset = sizeof(plength) + sizeof(plength) * num_packets;
            //TODO: describe size offset. it's basically the location of the next "packet size" in the buffer
            size_t size_offset = sizeof(plength);
            char tmp_buf[BUFSIZE];
            for (int packet = 0; packet < num_packets; packet++) {
                do_debug("Attempting to write %dth packet\n", packet);
                //get the size of this packet
                memcpy((char*) &plength, buffer+size_offset, sizeof(plength));
                size_offset += sizeof(plength);
                //get the buffer for this packet
                memcpy(tmp_buf, buffer+buffer_offset, ntohs(plength));
                buffer_offset += ntohs(plength);

                /* nwrite = cwrite(tap_fd, buffer, nread); */
                nwrite = cwrite(tap_fd, tmp_buf, ntohs(plength));
                do_debug("NET2TAP %lu: Written %d bytes to the tap interface\n", net2tap, nwrite);

            }


            //so now we need to unwind the packet
            //first is the number of packets




            /* now buffer[] contains a full packet or frame, write it into the tun/tap interface */ 
            //no need to buffer the packet, because it is incoming so there is no need
            //nwrite = cwrite(tap_fd, buffer, nread);
            //do_debug("NET2TAP %lu: Written %d bytes to the tap interface\n", net2tap, nwrite);
        }
    }

    return(0);
}
