/* *****************************************************************************
This file demonstrates the use of Linux namespaces 
  to create light-weight chroot jails.
It is intended primarily to clarify the use of namespaces
It draws on thoughts, code, and documentation from multiple CCS researchers
****************************************************************************** */

/* ************ Conceptual outline *******
Given (1) a root directory, (2) a current directory, and (3) a program to run from within this directory
create a Linux namespace in which 
  the given directory is the *root* of execution,
  suitable users and groups are created, 
    At this point the only user is userid 0 - thus apparently root
    The internal root id gives access to the cask root directory but
      does NOT give superuser privileges (especially to host resources)
    The use of a QWIC qemu plugin with qemu user mode may be necessary to 
      give the illusion of super user privileges
  potentially some external directories are mapped into the root space
    In particular, if the environment variable CASK_QEMUDIR is set then 
      mount the directory specified in it as /qemu (at the root directory)
         it is assumed that qemu user mode executable for use in the cask are in the mounted directory
      create a lib64 directory for use by the qemu executables as well as a lib/x865_64-linux-gnu
      This supports the primary use case of running arbitrary architecture binaries (i.e. often not x86)
         on an x86 host.  If the host is not x86 than modifications will be necessary
    Also by default the external /dev and /proc are mounted into the root directory
      This behavior can be modified with the MOUNT_DEV and MOUNT_PROC defined variables
  the desired program is launched from the given current direcory
********************/


// A variety of required Linux header files
#define _GNU_SOURCE
#include <sys/types.h> 
#include <sys/socket.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <signal.h> 
#include <sys/mount.h> 
#include <sys/wait.h> 
#include <sched.h> 
#include <string.h> 
#include <errno.h> 

// This is utility code 
/* Recursive mkdir - 
   thanks to Jonathon Reinhart in the internet 
   Modified to allow specifying mode for final directory
*/
/* Make a directory; already existing dir okay */
static int maybe_mkdir(const char* path, mode_t mode)
{
    struct stat st;
    errno = 0;

    //printf("Try make of %s for mode %o ",path,mode);
    /* Try to make the directory */
    if (mkdir(path, mode) == 0) {
        //printf(" succeeded\n");
        return 0;
    }

    /* If it fails for any reason but EEXIST, fail */
    if (errno != EEXIST) {
        //printf(" failed\n");
        return -1;
    }

    //printf(" existed ");
    /* Check if the existing path is a directory */
    if (stat(path, &st) != 0) {
        //printf(" but no stat\n");
        return -1;
    }

    /* If not, fail with ENOTDIR */
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
	//printf(" but not a direcotry\n");
        return -1;
    }

    //printf(" so okay\n");

    errno = 0;
    return 0;
}

int mkdir_p(const char *path, mode_t mode)
{
    /* Adapted from http://stackoverflow.com/a/2336245/119527 */
    char *_path = NULL;
    char *p; 
    int result = -1;

    errno = 0;

    /* Copy string so it's mutable */
    _path = strdup(path);
    if (_path == NULL)
        goto out;

    /* Iterate the string */
    for (p = _path + 1; *p; p++) {
        if (*p == '/') {
            /* Temporarily truncate */
            *p = '\0';

            if (maybe_mkdir(_path, 0777) != 0)
                goto out;

            *p = '/';
        }
    }   

    if (maybe_mkdir(_path, mode) != 0)
        goto out;

    result = 0;

out:
    free(_path);
    return result;
}


void domount(const char* rootdir, const char* internaldirname, const char* externaldirname) {
  char scratch[2000];
  int ret;
  int i;
  // Make sure have mount point in internal root dir
  sprintf(scratch,"%s/%s",rootdir,internaldirname);
  ret= mkdir_p(scratch,0555);
  if((ret == -1) && (errno != EEXIST)){
    // show failures other than file exists
    fprintf(stderr,"Mkdir of %s as mount point failed (%d) (%s)\n",scratch,errno, strerror(errno));
  }
  ret= mount(externaldirname,scratch,NULL,MS_MGC_VAL|MS_BIND|MS_REC|MS_RDONLY,NULL);
  if(ret!=0){
    fprintf(stderr,"mount of %s on %s failed (%d) (%s)\n",internaldirname,externaldirname, errno, strerror(errno));
    exit(11);
  } else {
    //fprintf(stderr,"mount of %s on %s succeeded\n",externaldirname,scratch);
  }
}

// End utility code

// A small amount of shared data between parent and child
#define STACK_SIZE (1024*1024)
static char**sharedenvp;
static int sockets[2];
// A generic signature for a child to keep compiler happy.
int child_code(void *arglist);

// Some default values - perhaps should draw from ENV variables of same name
// What userid to have in cask
#define DEFAULT_USERID 0

// mount /dev from outside to inside
#define MOUNT_DEV 1
// mount /proc from outside to inside
#define MOUNT_PROC 1



int main(int argc,char**argv,char**envp) {
  char*stack;
  char*stacktop;
  int flags;
  int childpid;
  char scratch[2000];
  int fd;

  // Make sure stdout is unbuffered
  setbuf(stdout,NULL);
  // Create a socketpair so parent can sync with child
  if(socketpair(AF_UNIX,SOCK_STREAM,0,sockets)<0){
    perror("opening stream socket pair");
    exit(1);
  }
  // Make a small stack for use by child until is execs desired program
  stack = malloc(STACK_SIZE);
  if(stack==NULL){
    perror("child stack alloc failed");
    exit(1);
  }
  stacktop = stack+STACK_SIZE;

  // Flags to cause clone to create new namespace
  //   Also get new pid, user, etc...
  flags = CLONE_NEWIPC|CLONE_NEWNS|CLONE_NEWPID|CLONE_NEWUSER|CLONE_NEWUTS|SIGCHLD;

  // save environment pointer for child to use
  sharedenvp = envp;
  // Create the child in the new namespace - pass along all the arguments but our name
  childpid = clone(child_code,stacktop,flags,argv+1);
  if(childpid==-1){
    perror("clone failed");
    exit(1);
  }

  // Now customize the namespace
  // 1. Make a root user (0) inside the namespace which maps out to users id
  sprintf(scratch,"/proc/%d/uid_map",childpid);
  fd= open(scratch,O_WRONLY);
  if(fd==-1){
    printf("unable to open uid map\n");
    kill(childpid,SIGTERM);
    exit(2);
  }
  sprintf(scratch,"%d %06d 1\n",DEFAULT_USERID,getuid());
  write(fd,scratch,11);
  close(fd);
  if(0){
    printf("Set child %d uid for %d to 0\n%s",
	   childpid,getuid(),scratch);
  }

  // 2. Make sure groups do not become a security hole
  //    and add a single group id,
  //    adding more than one seems to fail
  sprintf(scratch,"/proc/%d/setgroups",childpid);
  fd= open(scratch,O_WRONLY);
  if(fd==-1){
    printf("unable to open setgroups\n");
    kill(childpid,SIGTERM);
    exit(2);
  }
  write(fd,"deny\n",5);
  close(fd);
  if(0){
    printf("Set groups %d to deny\n",childpid);
  }

  sprintf(scratch,"/proc/%d/gid_map",childpid);
  fd= open(scratch,O_WRONLY);
  if(fd==-1){
    printf("unable to open gid map\n");
    kill(childpid,SIGTERM);
    exit(2);
  }
  sprintf(scratch,"0 %06d 1\n",getgid());
  write(fd,scratch,11);
  close(fd);
  if(0){
    printf("Set child %d gid for %d to 0\n%s",
	   childpid,getgid(),scratch);
  }

  // Tell child wrapper that it can proceed
  //   namespace all set up
  write(sockets[1],"go",2);


  // Cleanup - report how 
  pid_t cpid;
  int status;

  cpid= waitpid(childpid,&status,0);
  if(cpid<=0){
    perror("parent waitpid failed");
    exit(1);
  }
  if(WIFEXITED(status)){
    if(status!=0){
      printf("exited, status=%d\n",WEXITSTATUS(status));
    }
  } else if(WIFSIGNALED(status)){
    printf("killed by signal %d\n",WTERMSIG(status));
  } else if(WIFSTOPPED(status)){
    printf("stopped by signal %d\n",WSTOPSIG(status));
  } else if(WIFCONTINUED(status)){
    printf("continued\n");
  }
  if(0){
    printf("Parent sees child %d complete with status %d\n",cpid,status);
  }
}


/* A wrapper to run within the chroot jail
   Can do bind mounts before doing the execve which starts desired program.
 */
int child_code(void* arglist){
  char okmsg[3];
  int ret;
  int i;

  char**args;
  char*rootdir;
  char*cmd;
  char*curdir;
  char**cmdargs;

  // Wait for parent to send sync message that namespace setup complete
  okmsg[2]= 0;
  read(sockets[0],okmsg,2);
  if(0){
    printf("Child got ok %s\n",okmsg);
  }

  // This wrapper interprets its arglist as an array of pointers to arguments
  //  The first arg is the root directory name
  //    Ultimatedly chroot will be used to (securely) base all execution in this directory
  //    Any external directories to be bound in will be hosted below this directory
  //  The second argument is a configuration string - what directories to mount
  //  The third argument is the program to run
  //  All remaining arguments are passed to the program as arguments
  args = (char**)arglist;
  rootdir = args[0];
  curdir = args[1];
  cmd = args[2];
  cmdargs = args+2;
  if(0){
    printf("chroot to %s\n",rootdir);
    printf("chdir to %s\n",curdir);
    printf("run cmd %s\n",cmd);
  }

  const char *externalqemudir = getenv("CASK_QEMUDIR");
  if (externalqemudir != NULL)  {
    if (!(getenv("SKIP_MOUNT_LIB64"))) {
      domount(rootdir,"lib64","/lib64");
    }
    if (!(getenv("SKIP_MOUNT_X86_64"))) {
      domount(rootdir,"lib/x86_64-linux-gnu","/lib/x86_64-linux-gnu");
    }

    domount(rootdir,"qemu",externalqemudir);
  }
  if(getenv("MOUNT_DEV")){
    domount(rootdir,"dev","/dev");
  }

    if(getenv("MOUNT_PROC")){
    domount(rootdir,"proc","/proc");
  }

  ret= chroot(rootdir);
  if(ret!=0){
    perror("cask: Chroot failed");
    exit(22);
  }

  ret= chdir(curdir);
  if(ret!=0){
    perror("cask: Chdir to current dir failed");
  } else {
    //fprintf(stderr,"cask: chdir to (%s) succeeded\n",curdir);
  }
    

  ret= access(cmd,F_OK);
  if(ret!=0){
    fprintf(stderr,"cask: %s does not exist (%s)\n",cmd,strerror(errno));
    exit(-2);
  }

  ret= access(cmd,X_OK);
  if(ret!=0){
    fprintf(stderr,"cask: No execute access to %s (%s)\n",cmd,strerror(errno));
    exit(-2);
  } else {
    //fprintf(stderr,"cask: cmd %s in curdir %s is executable\n",cmd,curdir);
  }

  ret= access(cmdargs[0],X_OK);
  if(ret!=0){
    fprintf(stderr,"cask: No execute access to %s (%s)\n",cmdargs[0],strerror(errno));
    exit(-2);
  } else {
    //fprintf(stderr,"cask: cmd %s in curdir %s is executable\n",cmdargs[0],curdir);
  }

  for(i= 0;i<10;i++){
    if(cmdargs[i]==NULL)break;
    printf(" %s",cmdargs[i]);
  }
  printf("\n");

  ret= execve(cmd,cmdargs,sharedenvp);
  if(ret!=0){
    fprintf(stderr,"cask: Execve of %s failed (%s)\n",cmd,strerror(errno));
    exit(-2);
  }

  return 15;
}
