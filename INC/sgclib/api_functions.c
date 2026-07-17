/**
 *  SD card access module for Saturn Gamer's Cartridge
 *  by cafe-alpha
 *
 *  See LICENSE file for details.
 **/

#include "sgclib.h"

#include "fatfs/ff.h"

#include "log.h"
#include "sdcard.h"

FATFS FatFs = {0}; /* File system object for each logical drive */

/* -------------------------------------------------------------------------- */
int sgc_init(void) {
  log_out("sgclib_api_init STT");

  log_init();

  /* Initialize low-level access to SD card. */

  /* Initialize FatFs internals. */
  FRESULT res = f_mount(&FatFs, "", 1);
  log_out("sgclib_api_init f_mount:%d", res);

  log_out("sgclib_api_init END");

  return res;
}

/* -------------------------------------------------------------------------- */
// file handles {{{
#define MAX_FILES 2
static FIL files[MAX_FILES];
static int files_open = 0;

FIL *get_fil_from_fd(int fd) {
  log_out("get_fil_from_fd(%d)", fd);

  if ((fd < 0) || (fd >= MAX_FILES)) {
    return NULL;
  }

  if (!(files_open & (1 << fd))) {
    return NULL;
  }

  return &files[fd];
}

int alloc_fd(void) {
  int fd = 0;
  for (fd = 0; fd < MAX_FILES; fd++) {
    if (!(files_open & (1 << fd))) {
      files_open |= (1 << fd);
      return fd;
    }
  }
  return -1;
}

void free_fd(int fd) {
  if ((fd < 0) || (fd >= MAX_FILES)) {
    return;
  }

  files_open &= ~(1 << fd);
}

/* -------------------------------------------------------------------------- */

// Given a filename and some FA_xxx flags, return a file descriptor
// (or a negative error corresponding to SGC_FR_xxx)
int sgc_open(const char *filename, int flags) {
  int fd = alloc_fd();

  if (fd < 0) {
    return -FR_TOO_MANY_OPEN_FILES;
  }

  FIL *f = get_fil_from_fd(fd);

  FRESULT res = f_open(f, filename, flags);
  if (res != FR_OK) {
    free_fd(fd);
    return -1;
  }

  return fd;
}

// Close a file descriptor
int sgc_close(int fd) {
  FIL *f = get_fil_from_fd(fd);
  if (!f) {
    return -SGC_FR_INVALID_OBJECT;
  }

  int res = f_close(f);

  free_fd(fd);

  return res;
}

// Seek to a byte on an fd. Returns the offset
int sgc_seek(int fd, int offset, int whence) {
  FIL *f = get_fil_from_fd(fd);
  if (!f) {
    return -SGC_FR_INVALID_OBJECT;
  }

  f_sync(f);

  switch (offset) {
  case C_SEEK_SET:
    f_lseek(f, offset);
    break;
  case C_SEEK_CUR:
    f_lseek(f, offset + f_tell(f));
    break;
  case C_SEEK_END:
    f_lseek(f, offset + f_size(f));
    break;
  }

  return f_tell(f);
}

// Read some data. Returns bytes read
int sgc_read(int fd, void *buf, int len) {
  if (len < 0) {
    return -FR_INVALID_PARAMETER;
  }

  FIL *f = get_fil_from_fd(fd);
  if (!f) {
    return -SGC_FR_INVALID_OBJECT;
  }

  UINT nread = 0;
  f_read(f, buf, len, &nread);

  return (int)nread;
}

// Write some data. Returns bytes written
int sgc_write(int fd, const void *buf, int len) {
  if (len < 0) {
    return -FR_INVALID_PARAMETER;
  }

  FIL *f = get_fil_from_fd(fd);
  if (!f) {
    return -SGC_FR_INVALID_OBJECT;
  }

  // if (length > sizeof(buffer))
  //     goto fail;

  UINT nwritten = 0;
  f_write(f, buf, len, &nwritten);

  return (int)nwritten;
}

// Flush any buffered data to file
int sgc_sync(int fd) {
  // FIL* f = get_fil_from_fd(fd);
  // if(!f)
  ///{
  //    return -SGC_FR_INVALID_OBJECT;
  //}
  // return f_sync(f);

  return sgc_seek(fd, 0, C_SEEK_CUR);
}

// Truncate file at current pointer. Returns new length
int sgc_truncate(int fd) {
  FIL *f = get_fil_from_fd(fd);
  if (!f) {
    return -SGC_FR_INVALID_OBJECT;
  }

  f_truncate(f);

  return f_tell(f);
}

// Get info on a named file. Pass in a pointer to a buffer and
// its size - the filename may be truncated if the buffer is
// short.  Returns the length of the (truncated) filename.
// If the filename is NULL, reads the next file from the
// current directory (readdir).
int sgc_stat(const char *filename, sgc_stat_t *stat, int statsize) {
  FILINFO fstat = {0};

  log_out("sgc_stat STT ...");

  int ret = f_stat(filename, &fstat);

  /* Copy information from FatFs structure to sgclib structure. */
  stat->size = fstat.fsize;
  stat->date = fstat.fdate;
  stat->time = fstat.ftime;
  stat->attrib = fstat.fattrib;
  memcpy(stat->name, fstat.fname, sizeof(stat->name));
  stat->name[sizeof(stat->name) - 1] = '\0';

  log_out("sgc_stat END ... size[%u]", fstat.fsize);

  return ret;
}

// Rename a file.
int sgc_rename(const char *old, const char *new) { return f_rename(old, new); }

// Create a directory.
int sgc_mkdir(const char *filename) { return f_mkdir(filename); }

// Delete a file.
int sgc_unlink(const char *filename) { return f_unlink(filename); }

DIR dir;
int diropen = 0;

// Open a directory to read file entries.
int sgc_opendir(const char *path) {
  if (diropen) {
    f_closedir(&dir);
    diropen = 0;
  }

  if (f_opendir(&dir, path) == FR_OK) {
    diropen = 1;
  }

  return SGC_FR_OK;
}

// Change working directory.
int sgc_chdir(const char *path) {
  f_chdir(path);
  return SGC_FR_OK;
}

// Get working directory. Adds terminating null.
int sgc_getcwd(char *buff, int buflen) {
  f_getcwd(buff, buflen);
  buff[buflen] = '\0';

  return strlen(buff);
}
