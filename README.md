# bitmap2pbm

*Creates a P4 type PBM image from a binary file. With this program you can visualize your binary (for example a usage map).*

For example creating an image of the first 10000 bytes of memtest binary:
```head -c 10000 /boot/memtest86+-4.20 | bitmap2pbm --of memtest.pbm```

Result:

![Image of memtest86](http://fejlesztek.hu/wp-content/uploads/2013/11/bitmap2pbm.png)

Converted to PNG by [Gimp](http://www.gimp.org/).

## Usage

```bitmap2pbm [options]```

**Options:**

* **--aspect NUM**
Aspect ratio of image (default: 4:3)
The float result of width/height division.
* **--width NUM**
Image width in pixels (multiple of 8)
The width must be divisible by 8 because of the image format limitation.
* **--height NUM**
Image height in pixels
* **--if FILE**
Specifies the input file (default: stdin).
* **--of FILE**
Specifies the output file (default: stdout)
* **--bs NUM**
Block size to read/write
* **--help**
Prints usage information.
* **--version**
Prints version information.

Program supports stdout/stdin redirections, so the following should work:
```cat usagemap.dat | bitmap2pbm --of image.pbm```

However because the size of the input determines the header of output, you cannot redirect stdout and stdin simultaneously because they are not seekable.

You can get progress information by sending a SIGUSR1 signal to the process:
```
kill -SIGUSR1 pid
``` (where pid is the process identifier)

## Build
Program should build on any UNIX like operation system with a standard C compiler and make utility.

To compile:
```
make
```

To compile without signal handling:
```
make CFLAGS='-DNOSIGNAL'
```

To run:
```
./bin/bitmap2pbm
```
(however it is not recommended to call this way because it will wait for input from stdin)

To clean:
```
make clean
```

## Author
Written by Andras Majdan.

License: GNU General Public License Version 3

Report bugs to <majdan.andras@gmail.com>

## See also
[bitmapdd](https://github.com/andmaj/bitmapdd) - Creates a bitmap from a file (or device).

[fat2bitmap](https://github.com/andmaj/fat2bitmap) - Creates a bitmap from FAT file system free/used clusters. 
