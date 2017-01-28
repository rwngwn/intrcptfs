#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <netdb.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <pwd.h>
#include <grp.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "libc.h"
#include "fcall.h"
#include "9pfs.h"
#include "util.h"

#define HOST "HOST"
#define PORT "PORT"
//#use hasfilename for caching
//# look at the getcwd

// gcc -ldl -fPIC -shared -o libintrcptfs.so.0.1  test.c
static int (*real_open)(const char* pathname, int flags, ...) = NULL;
// we need to hook __lxstat function because of some glibx stat calls magic
static int (*real_lxstat)(int ver, const char *pathname, struct stat *buf) = NULL;
static int (*real_xstat)(int ver, const char *pathname, struct stat *buf) = NULL;
static int (*real_chdir)(const char *path) = NULL;
static int (*real_execve)(const char *filename, char *const argv[], char *const envp[]) = NULL;

static int connected = 0;
enum
{
	CACHECTLSIZE = 8, /* sizeof("cleared\n") - 1 */
	MSIZE = 8192
};


void
dir2stat(struct stat *s, Dir *d)
{
  struct passwd	*p;
  struct group	*g;

  s->st_dev = d->dev;
  s->st_ino = d->qid.path;
  s->st_mode = d->mode & 0777;
  if(d->mode & DMDIR)
    s->st_mode |= S_IFDIR;
  else
    s->st_mode |= S_IFREG;
  s->st_nlink = 1;
  s->st_uid = (p = getpwnam(d->uid)) == NULL ? 0 : p->pw_uid;
  s->st_gid = (g = getgrnam(d->gid)) == NULL ? 0 : g->gr_gid;
  s->st_size = d->length;
  s->st_blksize = msize - IOHDRSZ;
  s->st_blocks = d->length / (msize - IOHDRSZ) + 1;
  s->st_atime = d->atime;
  s->st_mtime = s->st_ctime = d->mtime;
  s->st_rdev = 0;
}	


FFid* _connect(const char *host, const char *port){
 
  struct sockaddr *addr;
  struct passwd *pw;
  struct addrinfo *ainfo;
  
  char  user[40];
  int alen, e;
  
  if((e = getaddrinfo(host, port, NULL, &ainfo)) != 0)
    errx(1, "%s", gai_strerror(e));
  addr = ainfo->ai_addr;
  alen = ainfo->ai_addrlen;

  if((pw = getpwuid(getuid())) == NULL)
    errx(1, "Could not get user");
  strecpy(user, user+sizeof(user), pw->pw_name);

  srvfd = socket(addr->sa_family, SOCK_STREAM, 0);
  if(connect(srvfd, addr, alen) == -1)
    err(1, "Could not connect to 9p server");
  init9p();
  msize = _9pversion(MSIZE);

  rootfid = _9pattach(ROOTFID, NOFID, user, NULL);
  return rootfid;
}

int _stat(char * path, struct stat* st){
  FFid	*f;
  Dir	*d;

  if((f = _9pwalk(path)) == NULL)
    return -EIO;
  if((d = _9pstat(f)) == NULL){
    _9pclunk(f);
    return -EACCES;
  }
  dir2stat(st, d);
  _9pclunk(f);
  free(d);
  return 0; 
}

int _read(char * path, char * buf, size_t size, int fd){
  Fcall	tread, rread;
  tread.type = Tread;
  tread.offset = 0;
  tread.fid = fd;
  tread.count = size;
  if(do9p(&tread, &rread) == -1)
    return -1;
  memcpy(buf, rread.data, rread.count);
  return 0;
  
}

int open(const char *pathname, int flags, ...)
{
  struct stat remote_stat;
  int fd;
  int rc;
  void *buf;
  FFid *f;
  
  real_open = dlsym(RTLD_NEXT, "open");
  
  if (strncmp(pathname, "/xxx", 4) == 0){

    _connect(getenv(HOST), getenv(PORT));

    rc = _stat(pathname+4, &remote_stat);
    printf("%d, %s\n", rc, pathname+4);
    printf("remote file size: %d\n", remote_stat.st_size);
    buf = malloc(remote_stat.st_size);

    if((f = _9pwalk(pathname+4)) == NULL)
      return ENOENT;

    if(_9popen(f) == -1){
      _9pclunk(f);
      return EACCES;
    }
    rc = _9pread(f, buf, remote_stat.st_size);
    printf("%s", buf);
    
    FILE * tmpfile = NULL;
    //FIXME - we are opening tempfile insted of real one to lets kernel give use proper file descriptor
    char * tmpname;

    tmpname = tmpnam(NULL);

    tmpfile = fopen(tmpname, "w");
    fwrite (buf , sizeof(char), remote_stat.st_size, tmpfile);
    fclose (tmpfile);
   
    fd = real_open(tmpname, flags);

    
    return fd;
  }
  
  return real_open(pathname,flags);
}

int execve(const char *filename, char *const argv[], char *const envp[]){
  struct stat remote_stat;
  int fd;
  int rc;
  void *buf;
  FFid *f;
  
  real_execve = dlsym(RTLD_NEXT, "execve");
  
  if (strncmp(filename, "/xxx", 4) == 0){
    _connect(getenv(HOST), getenv(PORT));

    rc = _stat(filename+4, &remote_stat);
    printf("%d, %s\n", rc, filename+4);
    printf("remote file size: %d\n", remote_stat.st_size);
    buf = malloc(remote_stat.st_size);

    if((f = _9pwalk(filename+4)) == NULL)
      return ENOENT;

    if(_9popen(f) == -1){
      _9pclunk(f);
      return EACCES;
    }
    rc = _9pread(f, buf, remote_stat.st_size);

    
    FILE * tmpfile = NULL;
    //FIXME - we are opening tempfile insted of real one to lets kernel give use proper file descriptor
    char * tmpname;

    tmpname = tmpnam(NULL);

    tmpfile = fopen(tmpname, "w");
    fwrite (buf , sizeof(char), remote_stat.st_size, tmpfile);
    fclose (tmpfile);
    chmod(tmpname, 0555);
    printf("executing %s", tmpname);
    return real_execve(tmpname, argv, envp);

    
  }
  return real_execve(filename, argv, envp);
}


int __xstat(int ver, const char *pathname, struct stat *buf)
{
  real_xstat = dlsym(RTLD_NEXT, "__xstat");
  if (strncmp(pathname, "/xxx", 4) == 0)
    {
      int rc;
      _connect(getenv(HOST), getenv(PORT));
      rc = _stat(pathname+4, buf);
      buf->st_mode = 00555;
      return rc;
    }
  int rc = real_xstat(ver, pathname, buf);
  return rc;
}  

int __lxstat(int ver, const char *pathname, struct stat *buf)
{
  real_lxstat = dlsym(RTLD_NEXT, "__lxstat");
  if (strncmp(pathname, "/xxx", 4) == 0)
    {
      int rc;
      _connect(getenv(HOST), getenv(PORT));
      rc = _stat(pathname+4, buf);
      buf->st_mode = 00555;
      return rc;
    }
  int rc = real_lxstat(ver, pathname, buf);
  return rc;
  

}

int chdir(const char *path) {

  real_chdir = dlsym(RTLD_NEXT, "chdir");
  if (strcmp(path, "/xxx") == 0)
    printf("MATCH");
    return 0;
  int rc = real_chdir(path);
  return rc;
}


