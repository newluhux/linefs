/* 
 * Compile with:
 *
 *     gcc -Wall linefs.c `pkg-config fuse3 --cflags --libs` -o linefs
*/ 

#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fuse.h>

char *filename = NULL;
int file_fd = -1;
FILE *file_fp = NULL;

char *log_filename = NULL;
FILE *log_fp = NULL;

#define MAXLINE  512 // max line length
#define MAXLINES 8192 // max of line nums

off_t line_offsets[MAXLINES];
size_t line_lengths[MAXLINES];
int line_nums = 0;

int line_info_fill(void) {
	char line[MAXLINE];
	off_t pos;
	while (fgets(line,MAXLINE,file_fp) != NULL) {
		if (line_nums >= MAXLINES)
			break;
		pos = ftell(file_fp) - strlen(line);
		line_offsets[line_nums] = pos;
		line_lengths[line_nums] = strlen(line);
		line_nums++;
	}
	return line_nums;
}

int line_info_print(void) {
	char line[MAXLINE];
	int i;
	off_t offset;
	size_t count;
	for(i=0;i<line_nums;i++) {
		offset = line_offsets[i];
		count = line_lengths[i];
retry:
		if (pread(file_fd,line,count,offset) != count)
			if (errno == EINTR)
				goto retry;
		line[count] = '\0';
		printf("%d %s",i+1,line);
	}
	return i;
}

static void *linefs_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg) {
	(void) conn;
	cfg->kernel_cache = 1;
	return NULL;
}

static int linefs_getattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi) {
	(void) fi;
	int res = 0;

	int val = atoi(path+1);

	memset(stbuf,0,sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0700;
		stbuf->st_nlink = 2;
	} else if (val > 0 && val <= line_nums) {
		stbuf->st_mode = S_IFREG | 0400;
		stbuf->st_nlink = 1;
		stbuf->st_size = line_lengths[val-1];
	} else {
		res = -ENOENT;
	}

	return res;
}

static int linefs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi,
		enum fuse_readdir_flags flags) {
	(void) offset;
	(void) fi;
	(void) flags;

	if (strcmp(path,"/") != 0)
		return -ENOENT;

	filler(buf,".",NULL, 0, 0);
	filler(buf,"..",NULL, 0, 0);
	int i;
	char num[MAXLINE];
	for (i=0;i<line_nums;i++) {
		snprintf(num,MAXLINE,"%d",i+1);
		filler(buf,num,NULL, 0, 0);
	}

	return 0;
}

static int linefs_open(const char *path,struct fuse_file_info *fi) {
	int i;
	int found = 0;
	char num[MAXLINE];
	for (i=0;i<line_nums;i++) {
		snprintf(num,MAXLINE,"%d",i+1);
		if (strcmp(num,path+1) == 0) {
			found = 1;
			break;
		}
	}
	if (found == 0)
		return -ENOENT;
	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int linefs_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi) {
	(void) fi;
	int i;
	int found = 0;
	char num[MAXLINE];
	for (i=0;i<line_nums;i++) {
		snprintf(num,MAXLINE,"%d",i+1);
		if (strcmp(num,path+1) == 0) {
			found = 1;
			if (offset < line_lengths[i]) {
				if (offset + size > line_lengths[i]) {
					size = line_lengths[i] - offset;
				}
				if (pread(file_fd,buf,size,line_offsets[i]+offset) != size) {
					return -1;
				}
			} else {
				size = 0;
			}

		}
	}
	if (found == 0)
		return -ENOENT;
	return size;
}

static const struct fuse_operations linefs_oper = {
	.init		=	linefs_init,
	.getattr	=	linefs_getattr,
	.readdir	=	linefs_readdir,
	.open		=	linefs_open,
	.read		=	linefs_read,
};

int main(int argc, char *argv[]) {
	filename = getenv("LINEFS_FILE");
	if (filename == NULL) {
		fprintf(stderr,"Please define environment variable : LINEFS_FILE\n");
		exit(EXIT_FAILURE);
	}

	/*
	log_filename = getenv("LINEFS_LOG_FILE");
	if (log_filename == NULL) {
		fprintf(stderr,"Please define environment variable : LINEFS_LOG_FILE\n");
		exit(EXIT_FAILURE);
	}
	*/

	struct stat stbuf;
	if (stat(filename,&stbuf) == -1) {
		fprintf(stderr,"stat() failed: %s\n",strerror(errno));
		exit(EXIT_FAILURE);
	}

	file_fd = open(filename,O_RDONLY);
	if (file_fd == -1) {
		fprintf(stderr,"open() failed: %s\n",strerror(errno));
		exit(EXIT_FAILURE);
	}

	file_fp = fdopen(file_fd,"r");
	if (file_fp == NULL) {
		fprintf(stderr,"fdopen() failed: %s\n",strerror(errno));
		exit(EXIT_FAILURE);
	}

	log_fp = fopen(log_filename,"w");
	if (log_fp == NULL) {
		fprintf(stderr,"fopen() failed: %s\n",strerror(errno));
		exit(EXIT_FAILURE);
	}

	line_info_fill();
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	int ret = fuse_main(args.argc, args.argv, &linefs_oper, NULL);
	fuse_opt_free_args(&args);

	/*
	fclose(log_fp);
	*/
	fclose(file_fp);
	close(file_fd);
	return ret;
}
