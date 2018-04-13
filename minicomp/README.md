# minicomp -- mini adaptive compression

This is the repo for the mini adaptive compression system inspired by Datacomp. Unlike datacomp, which considered bandwidth, CPU, and data type and chose from many compression options while also transparently performing compression on a file descriptor or socket, minicomp only considers data type and reads and writes data to and from buffers in a fashion similar to basic functions like `memcpy()` (making it suitable for adaptive compression where file descriptors are not being directly used, like networking or memory). It does not use any kind of training. If the data is compressible (as determined by bytecounting), the data is compressed using `zlib`. If the data is not estimated to be compressible, it is simply copied from the src to the destination. A header indicates "how" the resulting data was compressed -- using zlib or using "null compression" (copying).

Users of minicomp must provide source data and a destination buffer, which **must** be at least `sizeof(struct mcheader)` bytes *larger than the buffer `src`*. This is because a header is **always** written in output data to indicate how the data can later be decompressed.

# build requirements

```sudo apt-get install zlib1g-dev```

(... along with other generic gcc development tools.)

# building minicomp and the minitest testing program

`gcc minitest.c minicomp.c dc_bytecounter.c -o minitest -lz -lm`

This will produce the test and example program `minitest`. Run minitest by executing:

```./minitest```

To use in your own program, add the minicomp source files `dc_bytecounter.c` and `minicomp.c` to your tree, include `minicomp.h` in your program with the directive:

```#include "minicomp.h"```

... and add the source files `minicomp.c` and `dc_bytecounter.c` to your build command or makefile. 

Remember to also add the zlib library using the `-lz` gcc flag after installing the library.

# minitest

See the test program `minitest.c`. `main()` in `minitest.c` uses the function `mctest()` to test compress, decompress, and check two pieces of data in the buffer `raw`. `mctest()` uses the actual minicomp functions `minicomp()` and `minidecomp()`. One input buffer in `minitest` is very compressible and the other (read fresh from `/dev/urandom`) should never be compressible. Using minicomp, the result is that the first is compressed and decompressed. Since the second input is not compressible, minicomp just acts as though it is "compressing" it but in reality it just copies the data to the destination.

Regardless of whether data is compressed or not (null compression), the "compressed" data has a header added to it (defined as `struct mcheader` in `minicomp.h`). This header includes a magic value, a flag indicating whether the data was compressed, and the compressed and uncompressed sizes.

# minicomp functions

The main calls of are `minicomp()` and `minidecomp()`.

## minicomp

`minicomp()` is defined as:

```int minicomp(void *dest, const void *src, size_t len, size_t destlen);```

It checks the compressibility of the data in `src` and chooses whether or not to compress based on heuristics from datacomp. (It will also not compress if the input is < MINCOMPRESS as defined in `minicomp.h`.) If you consider "not compressing" to a form of compression called "null compression," minicomp will always compress `len` bytes in the buffer `src` and store them into `dest` along with a header indicating "how" the data was compressed. 

If the data is compressible, it will use zlib to compress the data. If the data is not compressible, it will use "null compression" (copying). Since `src` might not be compressible -- but a header must be appended either way for decompression -- `dest` should be longer than `src` by `sizeof(struct mcheader) + 64` and specified in the variable `destlen`. If the data is reasonably compressible, this addition will be more than made up for. If the data is uncompressible, this will result in a small waste of space.

`minicomp` returns the resulting size of the data after compression **including the header** (since the header will be necessary for decompression later).

For example, if 10 bytes are sent to be compressed by minicomp, they will not be compressed but will be copied. However, since decompression will require a mcheader, the return value of `minicomp()` will be 42 (10 + a 32-byte mcheader).

## minidecomp

`minidecomp()` is defined as:

```int minidecomp(void *dest, const void *src, size_t len, size_t destlen)```

`minidecomp` first looks at the header located at `src`. It verifies the magic value (for integrity) and then checks to see "how" the data was compressed (zlib or "null compression"). Then, it "decompresses" the data -- either using zlib or by copying the data from the src to the destination.

`minidecomp` returns the resulting size of the data after decompression (which should be the original size of the data).

## utility functions

All these functions take a pointer to a buffer (not a header):

 * `get_decomplen` -- get the uncompressed size (`ucsize`) of a mcbuffer
 * `get_complen` -- get the compressed size (`csize`) of a mcbuffer
 * `is_mcbuffer` -- returns 1 if a valid magic exists, else 0
 * `print_header` -- prints the header of a valid mcbuffer to stdout
 * `is_compressed` -- returns 1 if compressed, 0 if not

See `minicomp.h` for more information.

