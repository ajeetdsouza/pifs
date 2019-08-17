#define FUSE_USE_VERSION 26

#include <dirent.h>
#include <errno.h>
#include <fuse/fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/xattr.h>
#include <unistd.h>
#include "pi.hpp"

PiEncoder encoder{};
PiDecoder decoder{encoder};

struct options {
  char *rootdir;
} options;

static struct fuse_opt pifs_opts[] = {
    {"rootdir=%s", offsetof(struct options, rootdir), 0}};

static void pifs_fullpath(char fullpath[PATH_MAX], const char *path) {
  snprintf(fullpath, PATH_MAX, "%s%s", options.rootdir, path);
}

static int pifs_getattr(const char *path, struct stat *buf) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = lstat(fullpath, buf);
  buf->st_size /= 2;
  return ret == -1 ? -errno : ret;
}

static int pifs_readlink(const char *path, char *buf, size_t bufsiz) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = readlink(fullpath, buf, bufsiz - 1);
  if (ret == -1) {
    return -errno;
  }

  buf[ret] = '\0';
  return 0;
}

static int pifs_mknod(const char *path, mode_t mode, dev_t dev) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = mknod(fullpath, mode, dev);
  return ret == -1 ? -errno : ret;
}

static int pifs_mkdir(const char *path, mode_t mode) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = mkdir(fullpath, mode | S_IFDIR);
  return ret == -1 ? -errno : ret;
}

static int pifs_unlink(const char *path) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = unlink(fullpath);
  return ret == -1 ? -errno : ret;
}

static int pifs_rmdir(const char *path) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = rmdir(fullpath);
  return ret == -1 ? -errno : ret;
}

static int pifs_symlink(const char *oldpath, const char *newpath) {
  char fullnewpath[PATH_MAX];
  pifs_fullpath(fullnewpath, newpath);

  int ret = symlink(oldpath, fullnewpath);
  return ret == -1 ? -errno : ret;
}

static int pifs_rename(const char *oldpath, const char *newpath) {
  char fulloldpath[PATH_MAX];
  pifs_fullpath(fulloldpath, oldpath);

  char fullnewpath[PATH_MAX];
  pifs_fullpath(fullnewpath, newpath);

  int ret = rename(fulloldpath, fullnewpath);
  return ret == -1 ? -errno : ret;
}

static int pifs_link(const char *oldpath, const char *newpath) {
  char fulloldpath[PATH_MAX];
  pifs_fullpath(fulloldpath, oldpath);

  char fullnewpath[PATH_MAX];
  pifs_fullpath(fullnewpath, newpath);

  int ret = link(fulloldpath, fullnewpath);
  return ret == -1 ? -errno : ret;
}

static int pifs_chmod(const char *path, mode_t mode) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = chmod(fullpath, mode);
  return ret == -1 ? -errno : ret;
}

static int pifs_chown(const char *path, uid_t owner, gid_t group) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = chown(fullpath, owner, group);
  return ret == -1 ? -errno : ret;
}

static int pifs_truncate(const char *path, off_t length) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = truncate(fullpath, length * 2);
  return ret == -1 ? -errno : ret;
}

static int pifs_utime(const char *path, struct utimbuf *times) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = utime(fullpath, times);
  return ret == -1 ? -errno : ret;
}

static int pifs_open(const char *path, struct fuse_file_info *info) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = open(fullpath, info->flags);
  info->fh = ret;
  return ret == -1 ? -errno : 0;
}

static int pifs_read([[maybe_unused]] const char *path, char *buf, size_t count,
                     off_t offset, struct fuse_file_info *info) {
  int ret = lseek(info->fh, offset * 2, SEEK_SET);
  if (ret == -1) {
    return -errno;
  }

  for (size_t i = 0; i < count; i++) {
    uint16_t index;
    ret = read(info->fh, &index, sizeof(index));

    if (ret == -1) {
      return -errno;
    }
    if (ret == 0) {
      return i;
    }

    *buf = decoder[index];
    buf++;
  }

  return count;
}

static int pifs_write([[maybe_unused]] const char *path, const char *buf,
                      size_t count, off_t offset, struct fuse_file_info *info) {
  int ret = lseek(info->fh, offset * 2, SEEK_SET);
  if (ret == -1) {
    return -errno;
  }

  for (size_t i = 0; i < count; i++) {
    uint16_t index = encoder[*buf];

    ret = write(info->fh, &index, sizeof index);
    if (ret == -1) {
      return -errno;
    }
    buf++;
  }

  return count;
}

static int pifs_statfs(const char *path, struct statvfs *buf) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = statvfs(fullpath, buf);
  return ret == -1 ? -errno : ret;
}

static int pifs_release([[maybe_unused]] const char *path,
                        struct fuse_file_info *info) {
  int ret = close(info->fh);
  return ret == -1 ? -errno : ret;
}

static int pifs_fsync([[maybe_unused]] const char *path, int datasync,
                      struct fuse_file_info *info) {
  int ret = datasync ? fdatasync(info->fh) : fsync(info->fh);
  return ret == -1 ? -errno : ret;
}

static int pifs_setxattr(const char *path, const char *name, const char *value,
                         size_t size, int flags) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = setxattr(fullpath, name, value, size, flags);
  return ret == -1 ? -errno : ret;
}

static int pifs_getxattr(const char *path, const char *name, char *value,
                         size_t size) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = getxattr(fullpath, name, value, size);
  return ret == -1 ? -errno : ret;
}

static int pifs_listxattr(const char *path, char *list, size_t size) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = listxattr(fullpath, list, size);
  return ret == -1 ? -errno : ret;
}

static int pifs_removexattr(const char *path, const char *name) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = removexattr(fullpath, name);
  return ret == -1 ? -errno : ret;
}

static int pifs_opendir(const char *path, struct fuse_file_info *info) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  DIR *dir = opendir(fullpath);
  info->fh = (uint64_t)dir;
  return !dir ? -errno : 0;
}

static int pifs_readdir([[maybe_unused]] const char *path, void *buf,
                        fuse_fill_dir_t filler, off_t offset,
                        struct fuse_file_info *info) {
  const auto dir = reinterpret_cast<DIR *>(info->fh);
  if (offset) {
    seekdir(dir, offset);
  }

  int ret;
  do {
    errno = 0;
    struct dirent *de = readdir(dir);
    if (!de) {
      if (errno) {
        return -errno;
      } else {
        break;
      }
    }

    ret = filler(buf, de->d_name, nullptr, de->d_off);
  } while (ret == 0);

  return 0;
}

static int pifs_releasedir([[maybe_unused]] const char *path,
                           struct fuse_file_info *info) {
  int ret = closedir(reinterpret_cast<DIR *>(info->fh));
  return ret == -1 ? -errno : ret;
}

static int pifs_fsyncdir([[maybe_unused]] const char *path, int datasync,
                         struct fuse_file_info *info) {
  int fd = dirfd(reinterpret_cast<DIR *>(info->fh));
  if (fd == -1) {
    return -errno;
  }

  int ret = datasync ? fdatasync(fd) : fsync(fd);
  return ret == -1 ? -errno : ret;
}

static int pifs_access(const char *path, int mode) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = access(fullpath, mode);
  return ret == -1 ? -errno : ret;
}

static int pifs_create(const char *path, mode_t mode,
                       struct fuse_file_info *info) {
  char fullpath[PATH_MAX];
  pifs_fullpath(fullpath, path);

  int ret = creat(fullpath, mode);
  info->fh = ret;
  return ret == -1 ? -errno : 0;
}

static int pifs_ftruncate([[maybe_unused]] const char *path, off_t length,
                          struct fuse_file_info *info) {
  int ret = ftruncate(info->fh, length * 2);
  return ret == -1 ? -errno : ret;
}

static int pifs_fgetattr([[maybe_unused]] const char *path, struct stat *buf,
                         struct fuse_file_info *info) {
  int ret = fstat(info->fh, buf);
  return ret == -1 ? -errno : ret;
}

static int pifs_lock([[maybe_unused]] const char *path,
                     struct fuse_file_info *info, int cmd, struct flock *lock) {
  int ret = fcntl(info->fh, cmd, lock);
  return ret == -1 ? -errno : ret;
}

static int pifs_utimens(const char *path, const struct timespec times[2]) {
  DIR *dir = opendir(options.rootdir);
  if (!dir) {
    return -errno;
  }
  int ret = utimensat(dirfd(dir), basename(const_cast<char *>(path)), times, 0);
  closedir(dir);
  return ret == -1 ? -errno : ret;
}

static struct fuse_operations pifs_ops = {
    .getattr = pifs_getattr,
    .readlink = pifs_readlink,
    .mknod = pifs_mknod,
    .mkdir = pifs_mkdir,
    .unlink = pifs_unlink,
    .rmdir = pifs_rmdir,
    .symlink = pifs_symlink,
    .rename = pifs_rename,
    .link = pifs_link,
    .chmod = pifs_chmod,
    .chown = pifs_chown,
    .truncate = pifs_truncate,
    .utime = pifs_utime,
    .open = pifs_open,
    .read = pifs_read,
    .write = pifs_write,
    .statfs = pifs_statfs,
    .release = pifs_release,
    .fsync = pifs_fsync,
    .setxattr = pifs_setxattr,
    .getxattr = pifs_getxattr,
    .listxattr = pifs_listxattr,
    .removexattr = pifs_removexattr,
    .opendir = pifs_opendir,
    .readdir = pifs_readdir,
    .releasedir = pifs_releasedir,
    .fsyncdir = pifs_fsyncdir,
    .access = pifs_access,
    .create = pifs_create,
    .ftruncate = pifs_ftruncate,
    .fgetattr = pifs_fgetattr,
    .lock = pifs_lock,
    .utimens = pifs_utimens,
    .flag_nullpath_ok = 1,
};

int main(int argc, char *argv[]) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  memset(&options, 0, sizeof(struct options));
  if (fuse_opt_parse(&args, &options, pifs_opts, nullptr) == -1) {
    return -1;
  }

  if (!options.rootdir) {
    fprintf(stderr,
            "%s: Metadata directory must be specified with -o "
            "rootdir=<directory>\n",
            argv[0]);
    return -1;
  }

  if (access(options.rootdir, R_OK | W_OK | X_OK) == -1) {
    fprintf(stderr, "%s: Cannot access metadata directory '%s': %s\n", argv[0],
            options.rootdir, strerror(errno));
    return -1;
  }

  int ret = fuse_main(args.argc, args.argv, &pifs_ops, nullptr);
  fuse_opt_free_args(&args);
  return ret;
}
