/*
 * Public header for archive_entry
 * Simplified for AVIF-Master integration
 */

#ifndef ARCHIVE_ENTRY_H_INCLUDED
#define ARCHIVE_ENTRY_H_INCLUDED

#include <sys/types.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct archive_entry;

/* Allocate and release */
struct archive_entry *archive_entry_new(void);
struct archive_entry *archive_entry_clone(struct archive_entry *);
void archive_entry_free(struct archive_entry *);

/* Filename and size */
void archive_entry_set_pathname(struct archive_entry *, const char *);
const char *archive_entry_pathname(struct archive_entry *);
void archive_entry_set_size(struct archive_entry *, int64_t);
int64_t archive_entry_size(struct archive_entry *);

/* File type */
void archive_entry_set_filetype(struct archive_entry *, unsigned int);
unsigned int archive_entry_filetype(struct archive_entry *);

/* File permissions */
void archive_entry_set_perm(struct archive_entry *, mode_t);
mode_t archive_entry_perm(struct archive_entry *);

/* File ownership */
void archive_entry_set_uid(struct archive_entry *, int64_t);
int64_t archive_entry_uid(struct archive_entry *);
void archive_entry_set_gid(struct archive_entry *, int64_t);
int64_t archive_entry_gid(struct archive_entry *);

/* Timestamps */
void archive_entry_set_mtime(struct archive_entry *, time_t, long);
time_t archive_entry_mtime(struct archive_entry *);
long archive_entry_mtime_nsec(struct archive_entry *);

/* File type macros */
#define AE_IFMT     0170000
#define AE_IFREG    0100000
#define AE_IFLNK    0120000
#define AE_IFSOCK   0140000
#define AE_IFCHR    0020000
#define AE_IFBLK    0060000
#define AE_IFDIR    0040000
#define AE_IFIFO    0010000

#define AE_ISREG(m) ((m & AE_IFMT) == AE_IFREG)
#define AE_ISDIR(m) ((m & AE_IFMT) == AE_IFDIR)
#define AE_ISLNK(m) ((m & AE_IFMT) == AE_IFLNK)

#ifdef __cplusplus
}
#endif

#endif /* !ARCHIVE_ENTRY_H_INCLUDED */
