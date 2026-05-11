/*
 * Public header for libarchive
 * Simplified for AVIF-Master integration
 * Full version: https://github.com/libarchive/libarchive
 */

#ifndef ARCHIVE_H_INCLUDED
#define ARCHIVE_H_INCLUDED

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Type definitions */
typedef int64_t la_int64_t;

/* Opaque type */
struct archive;
struct archive_entry;

/* Error codes */
#define ARCHIVE_OK       0
#define ARCHIVE_EOF      1
#define ARCHIVE_RETRY    (-10)
#define ARCHIVE_WARN     (-20)
#define ARCHIVE_FAILED   (-25)
#define ARCHIVE_FATAL    (-30)

/* Format codes */
#define ARCHIVE_FORMAT_CPIO                  0x10000
#define ARCHIVE_FORMAT_CPIO_POSIX            (ARCHIVE_FORMAT_CPIO | 1)
#define ARCHIVE_FORMAT_SHAR                  0x20000
#define ARCHIVE_FORMAT_SHAR_BASE             (ARCHIVE_FORMAT_SHAR | 1)
#define ARCHIVE_FORMAT_SHAR_DUMP             (ARCHIVE_FORMAT_SHAR | 2)
#define ARCHIVE_FORMAT_TAR                   0x30000
#define ARCHIVE_FORMAT_TAR_USTART            (ARCHIVE_FORMAT_TAR | 1)
#define ARCHIVE_FORMAT_TAR_PAX               (ARCHIVE_FORMAT_TAR | 2)
#define ARCHIVE_FORMAT_TAR_GNU               (ARCHIVE_FORMAT_TAR | 3)
#define ARCHIVE_FORMAT_ZIP                   0x40000
#define ARCHIVE_FORMAT_7ZIP                  0x50000
#define ARCHIVE_FORMAT_RAR                   0x60000
#define ARCHIVE_FORMAT_CAB                   0x70000
#define ARCHIVE_FORMAT_RAR5                  0x80000

/* Create new archive object for reading */
struct archive *archive_read_new(void);

/* Specify the format to be read */
int archive_read_support_format_all(struct archive *);
int archive_read_support_format_zip(struct archive *);
int archive_read_support_format_7zip(struct archive *);
int archive_read_support_format_tar(struct archive *);

/* Specify decompression support */
int archive_read_support_filter_all(struct archive *);
int archive_read_support_filter_gzip(struct archive *);
int archive_read_support_filter_bzip2(struct archive *);
int archive_read_support_filter_xz(struct archive *);
int archive_read_support_filter_lzma(struct archive *);

/* Open for reading */
int archive_read_open_filename(struct archive *, const char *pathname,
    size_t block_size);

/* Read archive entries */
int archive_read_next_header(struct archive *, struct archive_entry **);
int archive_read_data(struct archive *, void *buff, size_t size);
int archive_read_data_into_fd(struct archive *, int fd);
int archive_read_data_block(struct archive *, const void **buff, size_t *size, 
                             la_int64_t *offset);

/* Close and free */
int archive_read_close(struct archive *);
int archive_read_free(struct archive *);

/* Get error info */
const char *archive_error_string(struct archive *);
int archive_errno(struct archive *);

/* ===== WRITING ===== */

struct archive *archive_write_new(void);

/* Specify format for writing */
int archive_write_set_format_zip(struct archive *);
int archive_write_set_format_7zip(struct archive *);
int archive_write_set_format_tar(struct archive *);

/* Specify compression */
int archive_write_set_filter_none(struct archive *);
int archive_write_set_filter_gzip(struct archive *);
int archive_write_set_filter_bzip2(struct archive *);
int archive_write_set_filter_xz(struct archive *);

/* Set compression level (0-9) */
int archive_write_set_filter_option(struct archive *, const char *m,
    const char *o, const char *v);

/* Open for writing to file */
int archive_write_open_filename(struct archive *, const char *pathname);

/* Write entries */
int archive_write_header(struct archive *, struct archive_entry *);
int archive_write_data(struct archive *, const void *buff, size_t size);
int archive_write_finish_entry(struct archive *);

/* Close and free */
int archive_write_close(struct archive *);
int archive_write_free(struct archive *);

/* Disk writer (for extraction to disk) */
struct archive *archive_write_disk_new(void);
int archive_write_disk_set_options(struct archive *, int flags);
int archive_write_disk_set_standard_lookup(struct archive *);
int archive_write_disk_open(struct archive *, const char *pathname);

/* Flags for archive_write_disk */
#define ARCHIVE_EXTRACT_OWNER     (0x0001)
#define ARCHIVE_EXTRACT_PERM      (0x0002)
#define ARCHIVE_EXTRACT_TIME      (0x0004)
#define ARCHIVE_EXTRACT_NO_OVERWRITE  (0x0008)
#define ARCHIVE_EXTRACT_UNLINK    (0x0010)
#define ARCHIVE_EXTRACT_ACL       (0x0020)
#define ARCHIVE_EXTRACT_FFLAGS    (0x0040)
#define ARCHIVE_EXTRACT_XATTR     (0x0080)
#define ARCHIVE_EXTRACT_SECURE_SYMLINKS (0x0100)
#define ARCHIVE_EXTRACT_SECURE_NODOTDOT (0x0200)
#define ARCHIVE_EXTRACT_NO_AUTODIR (0x0400)
#define ARCHIVE_EXTRACT_PRESERVE_PATHINFO (0x0800)

#ifdef __cplusplus
}
#endif

#endif /* !ARCHIVE_H_INCLUDED */
