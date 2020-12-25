#include <unicode.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

static int open_cwd(int *restrict fd);
static int prepare_output(int cwd_fd, const char *output, int *restrict out_fd);
static int compile_data(const int *restrict input, size_t size, int out_fd);
static int write_data(int out_fd, unsigned char idx, const int *data, size_t size);

int main(int argc, char **argv)
{
	int c;
	char *input = 0;
	char *output = 0;

	/* get command line options */
	if (__builtin_expect(argc < 2, 0)) {
		fputs("no input file\n", stderr);
		return -1;
	}

	while ((c = getopt(argc, argv, "o:")) != -1) {
		switch (c) {
		case 'o':
			output = optarg;
			break;
		case '?':
			if (optopt == 'o')
				fputs("Option -o requires an argument.\n", stderr);

			else if (isprint(optopt))
				fprintf(stderr, "Unknown option '-%c'.\n", optopt);

			else
				fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
			return -1;
		default:
			return -1;
		}
	}

	if (__builtin_expect(argc - optind != 1, 0)) {
		fputs("one input file expected\n", stderr);
		return -1;
	}

	if (__builtin_expect(output == 0, 0)) {
		fputs("output file not specified\n", stderr);
		return -1;
	}

	input = argv[optind];

	int rc, out_fd, cwd_fd;
	int *input_data;
	size_t input_size = 0;

	/* open the current directory */
	rc = open_cwd(&cwd_fd);
	if (__builtin_expect(rc != 0, 0))
		goto exit1;

	/* read input file as utf-8 and store the data in 'input_data' as utf-32 */
	rc = unicode_read_utf8_file(cwd_fd, input, &input_data, &input_size);
	if (__builtin_expect(rc != 0, 0))
		goto exit2;

	/* prepare the output directory that will contain all generated files */
	rc = prepare_output(cwd_fd, output, &out_fd);
	if (__builtin_expect(rc != 0, 0))
		goto exit3;

	/* perform the actual compiling task */
	rc = compile_data(input_data, input_size, out_fd);

	/* clean up */
	close(out_fd);
exit3:
	unicode_string_free(&input_data, 1);
exit2:
	close(cwd_fd);
exit1:
	return rc;
}

static int open_cwd(int *restrict fd)
{
	int rc;
	*fd = open(".", 0);

	if (__builtin_expect(*fd == -1, 0)) {
		rc = errno;
		fprintf(stderr, "cannot open current working directory. error %d\n", rc);
		return rc;
	}

	return 0;
}

static int prepare_output(int cwd_fd, const char *output, int *restrict out_fd)
{
	int rc = 0;
	struct stat st = {0};

	if (stat(output, &st) == -1) {
		if (__builtin_expect(mkdir(output, 0755) == -1, 0)) {
			rc = errno;
			fprintf(stderr, "cannot create directory '%s'. error %d\n", output, rc);
			goto exit1;
		}
	}

	*out_fd = openat(cwd_fd, output, 0);
	if (__builtin_expect(*out_fd == -1, 0)) {
		rc = errno;
		fprintf(stderr, "cannot open '%s'. error %d\n", output, rc);
	}

exit1:
	return rc;
}

static int compile_data(const int *restrict input, size_t size, int out_fd)
{
	/* FIXME: for debugging only */
	write_data(out_fd, 0, input, size);

	return 0;
}

static int write_data(int out_fd, unsigned char idx, const int *data, size_t size)
{
	char filename[16];
	snprintf(filename, sizeof(filename), "%d.html", (int) idx);
	return unicode_write_utf8_file(out_fd, filename, data, size);
}

