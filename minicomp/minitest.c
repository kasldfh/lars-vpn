#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include "minicomp.h"

#define BUFSIZE 1048576 * 1

void print_hex(char *buf, int len) {

	/* print len bytes of a buffer in hex */

	int i = 0;
	for (i = 0; i < len; i++) {

		printf("%02x ", buf[i]);

	}

	printf("\n");
}

void print_byte(char *buf, int len) {

	/* print len bytes of a buffer */

	int i = 0;
	for (i = 0; i < len; i++) {

		printf("%c", buf[i]);

	}

	printf("\n");
}

void fill_ab(char *buf, int len) {
	
	/* fill a buffer with 'ab' */

	int i;
	for (i = 0; i < len; i++) {
		if (i % 2 == 0) {
			buf[i] = 'a';
		} else {
			buf[i] = 'b';
		}
	}

}

int bufdiff(char *buf1, char *buf2, int len) {

	/* compare buffers byte by byte
	 * upon difference, return the index where chars differ
	 * upon match, return -1 
	 * -- it's weird, I know...
	 */

	int i;

	for (i = 0; i < len; i++) {

		if (buf1[i] != buf2[i]) {
			return i;
		}

	}

	return -1;

}

int mctest(char *raw, size_t len)
{

	printf("\nRunning a new test:\n");
	/* tests a buffer 'raw' of size BUFSIZE */

	int bufsz = len + sizeof(struct mcheader) + 64;

	char *check;
	check = (char *)malloc(BUFSIZE); /* decompressed data will go here */
	char *compressed; /* compressed data and header */
	compressed = (char *)malloc(bufsz);
	if (compressed == NULL) {
		printf("compressed is null!\n");
		abort();
	}

	/* point 'header' to beginning of buffer */
	struct mcheader *header = (struct mcheader*) compressed;
	/* compressed data will be _after the header_ */
	char *compressed_data = compressed + sizeof(struct mcheader);

	int ret;
	int diff;

	/* wipe compressed */
	memset((void *)compressed, 0, bufsz);
	
	/* zero out check to ensure decompression works */
	memset((void *)check, 0, BUFSIZE);

	/* print out raw prior to compression */
	printf("raw:\n");
	print_hex(raw, 40);
	print_byte(raw, 40);
	printf("\n");

	/* compress 'raw' and return the resulting size of the output
	 * which is now stored in 'compressed' */
	printf("\nCompressing data:\n");
	ret = minicomp(compressed, raw, len, bufsz);
	
	/* how big is the compressed version? */
	printf("minicomp completed. ret = %d\n", ret);

	/* what does the compressed data look like? */
	printf("compressed header:\n");
	print_hex(compressed, 75);
	printf("compressed data:\n");
	print_hex(compressed_data, 51);
	print_byte(compressed_data, 40);
	printf("\n");

	if (is_compressed(compressed)) {
		printf("Data is compressed.\n");
	} else {
		printf("Data is not compressed!\n");
	}
	print_header(compressed);

	/* decompress 'compressed_data' of len 'ret' into 'check' */
	printf("\nUncompressing data:\n");
	ret = minidecomp(check, compressed, ret, BUFSIZE);

	/* print out the decompressed size */
	printf("minidecomp completed. ret = %d\n", ret);

	/* print out the decompressed data */
	printf("decompressed:\n");
	print_hex(check, 40);
	print_byte(check, 40);
	printf("\n");

	diff = bufdiff(raw, check, len);

	if (diff >= 0) {

		printf("Buffers did not match at byte %d\n", diff);

	} else {
		printf("Buffers match!\n");
	}

	free(compressed);
	printf("Freed memory.\n");
	return ret;

}

int main(int argc, char *argv[])
{

	char *raw;
	raw = (char *)malloc(BUFSIZE);
	int i;

	printf("sizeof mcheader is: %lu\n", sizeof(struct mcheader));

	/* fill 'raw' with 'ab' pattern */
	fill_ab(raw, BUFSIZE);

	printf("test on 'ab' file\n");
	mctest(raw, BUFSIZE);
	
	FILE *fp;
	

	printf("test on random.txt\n");
	fp = fopen("random.txt", "r");
	for (i = 0; i < BUFSIZE; i++) {
		raw[i] = fgetc(fp);
	}
	fclose(fp);
	mctest(raw, BUFSIZE);


	printf("test on 10 bytes of the numbers file\n");
	fp = fopen("numbers.txt", "r");
	for (i = 0; i < BUFSIZE; i++) {
		raw[i] = fgetc(fp);
	}
	fclose(fp);
	mctest(raw, 10);

}
