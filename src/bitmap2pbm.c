/**
 * Creates a P4 type PBM image from a binary file.
 * 
 * Written by Andras Majdan
 * License: GNU General Public License Version 3
 * 
 * Report bugs to <majdan.andras@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>
#include <float.h>
#include <math.h>  

#ifndef NOSIGNAL
#include <signal.h>
#endif

#define DEFAULT_BS	512
#define VERSION "1.0.0"
#define PBM_COMMENT "# CREATOR: bitmap2pbm Version " VERSION

/* 18446744073709551616 18446744073709551616 (2^64) for storing
   width and height */
#define MAX_DIMENSION 20 + 1 + 20

/* 4:3 is the default aspect ratio */
#define DEFAULT_ASPECT ((double)4/3)

unsigned long long in_bytes = 0;
unsigned long long in_block = 0;
unsigned long long out_bytes = 0;
unsigned long long out_block = 0;

typedef enum { NONE = 0, ERROR, IO_ERROR } ERR_TYPE;

int run;

time_t start_time;

void print_progress(FILE *out)
{
	time_t end_time;
	double s;
	
	time(&end_time);
	s =  difftime(end_time, start_time);
	
	fprintf(out, 
			"%llu blocks in (%llu bytes)\n"
			"%llu blocks out (%llu bytes)\n"
			"%.0f s, %.1f MB/s\n",
			in_block, in_bytes, out_block, out_bytes, s, 
			(double)in_bytes/s/(double)1000000);
}

#ifndef NOSIGNAL
void sig_handler(int signum)
{
	if(signum==SIGUSR1) print_progress(stderr);
	if(signum==SIGINT) run = 0;
}
#endif

ERR_TYPE process_err(FILE *in, FILE *out, char *tofree, ERR_TYPE code, char *msg)
{
	fclose(in);
	fclose(out);
	if(tofree) free(tofree);
	
	if(msg != NULL) fprintf(stderr, "%s\n", msg);
	if(code == IO_ERROR) fprintf(stderr, "I/O error\n");
	
	if(code != NONE) 
	{
		print_progress(stderr);
		exit(code);
	}
	else
	{
		if(out != stdout) print_progress(stdout);
		return code;
	}
}

void check_bit_cut(uint64_t width, uint64_t height, off_t total)
{
	uint64_t cut;
	
	cut = total * 8 - (height * width);
	
	if(cut) fprintf(stderr, "%" PRIu64 " bits were cut\n", cut);
}

void str_right_shift(char *str, int len, int max)
{
	if(len == max) return;
	
	int i;
	for(i = len; i >= 0; i--, max--)
	{
		str[max] = str[i];
		str[i] = ' ';
	}
}

// Calculates the dimension of the bitmap
// Returns 0 on success
int calculate_dimension(
	uint64_t size,     // Size of input in bytes
	double *pAspect,   // Given aspect (or DBL_MAX)
	uint64_t *pWidth,  // Given width in pixels (or 0)
	uint64_t *pHeight) // Given height in pixels (or 0)
{
	double aspect = *pAspect;
	uint64_t width = *pWidth, height = *pHeight;
	
	if(!size) return 1;
	
	if(aspect != DBL_MAX) 
	{
		// Aspect is given
		if(width || height)
		{
			fprintf(stderr, "Aspect with width or height is given\n");
			return 1;
		}
	}
	else
	{
		// Aspect is not given
		aspect = DEFAULT_ASPECT;
	}
	
	if(width)
	{
		// Width is given
		if(width != (width/8)*8)
		{
			fprintf(stderr, "Width must be multiple of 8\n");
			return 1;
		}
		
		if(height)
		{
			// Width and height is given
			if(width * height > size * 8)
			{
				fprintf(stderr, "Width * height > input size\n");
				return 1;
			}
			// Use given width and height (do nothing)
		}
		else
		{
			// Width is given, height is not given
			height = (size*8)/width;
			if(height == 0)
			{
				fprintf(stderr, "Width is too large\n");
				return 1;
			}
		}
	}
	else
	{
		// Width is not given
		if(height)
		{
			// Width is not given, height is given
			// Calculate dimension using only the height
			width = (size*8)/height;
			width = (width/8)*8;
			if(width == 0)
			{
				fprintf(stderr, "Height is too large\n");
				return 1;
			}
		}
		else
		{
			// Neither width nor height is given
			// Calculate dimension using only the aspect
			width = sqrt( (size*8) * aspect );
			width = (width/8)*8;
			if(width == 0)
				width = 8;
			
			height = (size*8)/width;
		}
	}
	
	// Success
	*pAspect = aspect;
	*pWidth = width;
	*pHeight = height;
	return 0;
}

int bitmap2pbm(FILE *in, FILE *out,
	double aspect, uint64_t width, uint64_t height,
	unsigned long int bs)
{
	#ifndef NOSIGNAL
	signal(SIGUSR1, sig_handler);
	signal(SIGINT, sig_handler);
	#endif
	
	time(&start_time);

	off_t in_size = -1, dim_pos = -1;
	char p4_size[MAX_DIMENSION+1];
	char *block = NULL;
	
	memset(p4_size, ' ', MAX_DIMENSION);
	p4_size[MAX_DIMENSION] = '\0';
	
	/* Try to seek in input */
	if( !fseek(in, 0, SEEK_END) )
	{
		/* Input is seekable */
		in_size = ftell(in);
		
		if(in_size == -1)
			process_err(in, out, block, IO_ERROR, NULL);
		
		if( calculate_dimension(in_size, &aspect, &width, &height) )
			process_err(in, out, block, ERROR, "Fatal error");
		
		check_bit_cut(width, height, in_size);
		
		/* Write header to out */
		if( fprintf(out, "P4\n%s\n", PBM_COMMENT) < 0 )
			process_err(in, out, block, IO_ERROR, NULL);
		
		/* Write dimension to buffer */
		if( sprintf(p4_size, "%" PRIu64 " %" PRIu64 " ",
					width, height ) < 0 ) 
			process_err(in, out, block, IO_ERROR, NULL);
		
		/* Rewind to input start */
		if( fseek(in, 0, SEEK_SET) )
			process_err(in, out, block, IO_ERROR, NULL);
	}
	else
	{
		/* Input is not seekable */
		
		/* Try to seek in output stream */
		if( fseek(out, 0, SEEK_SET) )
			/* Not seekable output */
			process_err(in, out, block, ERROR,
				"Cannot determine input size and output is "
				"not seekable.");
		else
		{
			/* Write header to out */
			if( fprintf(out, "P4\n%s\n", PBM_COMMENT) < 0 )
				process_err(in, out, block, IO_ERROR, NULL);
			
			/* Save dimension starter position */
			dim_pos = ftell(out);
		}
	}
	
	/* Write dimension (or space for it) to out */
	if( fprintf(out, "%s", p4_size) < 0 )
		process_err(in, out, block, IO_ERROR, NULL);
	
	block = malloc(bs);
	if(block==NULL)
	{
		process_err(in, out, block, ERROR, 
			"Memory allocation has failed.");
	}
	
	size_t res_in, res_out;
	run = 1;
	
	while( run )
	{
		res_in = fread(block, sizeof(char), bs, in);
		if(res_in!=bs)
		{
			if( feof(in) )
				run = 0;
			else process_err(in, out, block, IO_ERROR, 
					"Input fread has failed");
		}
 
		if(res_in!=0)
		{
			++in_block;	
			res_out = fwrite(block, sizeof(char), res_in, out);
			if(res_out!=res_in) 
				process_err(in, out, block, IO_ERROR, 
					"Output fwrite has failed");
					
			++out_block;
			out_bytes += res_out;
			
		}
		in_bytes += res_in;
	}
	
	if( in_size == -1 )
	{
			if( calculate_dimension(in_bytes, &aspect, &width, &height) )
				process_err(in, out, block, ERROR, "Fatal error");
		
			check_bit_cut(width, height, in_bytes);
			
			int len;
			len = sprintf(p4_size, "%" PRIu64 " %" PRIu64,
					width, height);
			if(len < 0 ) 
				process_err(in, out, block, IO_ERROR, NULL);
				
			/* +1 to clear the first NULL character */
			str_right_shift(p4_size, len+1, MAX_DIMENSION);
		
			/* Seek to dimension start */
			if( fseek(out, dim_pos, SEEK_SET) ) 
				process_err(in, out, block, IO_ERROR, NULL);
			
			/* Write dimension to out */
			if( fprintf(out, "%s ", p4_size) < 0 )
				process_err(in, out, block, IO_ERROR, NULL);
	}
	

	return process_err(in, out, block, NONE, NULL);
}

int main(int argc, char *argv[])
{
	int c;
	char *endptr;
	
	double aspect;
	unsigned long int bs;
	uint64_t width, height;
	FILE *in, *out;
	
	/* Default values */
	bs = DEFAULT_BS;
	width = 0;
	height = 0;
	aspect = DBL_MAX;
	in = stdin;
	out = stdout;
 
	while(1)
	{
		static struct option long_options[] =
		{
			{"aspect",	required_argument, 0, 'a'},
			{"width",	required_argument, 0, 'x'},
			{"height",	required_argument, 0, 'y'},
			{"if", 		required_argument, 0, 'i'},
			{"of",    	required_argument, 0, 'o'},
			{"bs",    	required_argument, 0, 'b'},		
			{"help",    no_argument, 	   0, 'h'},
			{"version", no_argument, 	   0, 'v'},
			{0, 0, 0, 0}
		};
		
		int option_index = 0;
     
		c = getopt_long(argc, argv, "a:x:y:i:o:b:hv",
										long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;
     
		switch (c)
		{
			case 'a':
				aspect = strtod(optarg, &endptr);
				if(aspect == 0.0) aspect = DBL_MAX;
				break;
				
			case 'x':
				errno = 0;
				width = strtoull(optarg, &endptr, 10);
				if(width == 0 || *endptr!='\0' || errno!=0)
				{
					fprintf(stderr,
							"Wrong argument for width: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
				
			case 'y':
				errno = 0;
				height = strtoull(optarg, &endptr, 10);
				if(height == 0 || *endptr!='\0' || errno!=0)
				{
					fprintf(stderr,
							"Wrong argument for width: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
				
			case 'i':
				in = fopen(optarg, "r+b");
				if(in==NULL)
				{
					if(out!=stdout) fclose(out);
					fprintf(stderr,
							"Cannot open input file: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
				
			case 'o':
				out = fopen(optarg, "w+b");
				if(out==NULL)
				{
					if(in!=stdin) fclose(in);
					fprintf(stderr,
							"Cannot open output file: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
				
			case 'b':
				errno = 0;
				bs = strtoul(optarg, &endptr, 10);
				if(*endptr!='\0' || errno!=0)
				{
					fprintf(stderr,
							"Wrong argument for bs: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
			
			case 'h':
				printf(
					"Usage: bitmap2pbm [options]\n\n"
					
					"Creates a P4 type PBM image from a binary file.\n\n"
					
					"Options:\n"
					"  --aspect NUM  Aspect ratio of image (default: 4:3)\n"
					"  --width NUM   Image width in pixels (multiple of 8)\n"
					"  --height NUM  Image height in pixels\n"
					"  --if FILE     Input file (default: stdin)\n"
					"  --of FILE     Output file (default: stdout)\n"
					"  --bs NUM      Block size to read/write\n"
					"  --help        Usage information\n"
					"  --version     Print version\n\n"
 
					"Example 1: bitmap2pbm --if usagemap.dat --of image.pbm\n"
					"Example 2: cat usagemap.dat | bitmap2pbm --of image.pbm\n\n"

					"Written by Andras Majdan\n"
					"License: GNU General Public License Version 3\n"
					"Report bugs to <majdan.andras@gmail.com>\n");
				exit (EXIT_SUCCESS);
				break;
			case 'v':
				printf("Version: %s (Compiled on %s)\n", VERSION, __DATE__);
				exit (EXIT_SUCCESS);
				break;
			case '?':
				/* getopt_long already printed an error message. */
				exit(EXIT_FAILURE);
				break;
		}
	}
     
	/* Print any remaining command line arguments (not options). */
	if (optind < argc)
	{
		fprintf (stderr, "Invalid option(s): ");
		while (optind < argc)
			fprintf (stderr, "%s ", argv[optind++]);
		fprintf(stderr, "\n");
			
		exit(EXIT_FAILURE);
	}
	
	if( bitmap2pbm(in, out, aspect, width, height, bs) )
		return EXIT_FAILURE;
	else 
		return EXIT_SUCCESS;	
}
