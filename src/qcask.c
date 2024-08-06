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

#define STACK_SIZE (1024*1024)
static char**sharedenvp;
static int sockets[2];

// mount a directory for qemu inside container from given external directory
#define MOUNT_QEMU 1
#define QEMUEXTERNALDIR "/home/dsg/warez/QEMU/qemu/build-ub22/"
#define PUBEXTERNALDIR "/usr/public/opt"
// mount /dev from outside to inside
#define MOUNT_DEV 1
// mount /proc from outside to inside
#define MOUNT_PROC 1
// What userid to have in cask
#define DEFAULT_USERID 0
int child_code(void*);

void domount(const char* internaldirname, const char* externaldirname) {
  int ret;
  int i;
  // Make sure have mount point in internal root dir
  ret= mkdir(internaldirname,0555);
  if((ret == -1) && (errno != EEXIST)){
    // show failures other than file exists
    fprintf(stderr,"Mkdir of %s failed (%d) (%s)\n",internaldirname,errno, strerror(errno));
  }
  ret= mount(externaldirname,internaldirname,NULL,MS_MGC_VAL|MS_BIND|MS_REC|MS_RDONLY,NULL);
  if(ret!=0){
    fprintf(stderr,"mount of %s on %s (%d) (%s)\n",internaldirname,externaldirname, errno, strerror(errno));
    exit(11);
  } else {
    fprintf(stderr,"mount of %s on %s succeeded\n",externaldirname,internaldirname);
  }
}

int main(int argc,char**argv,char**envp) {
  char*stack;
  char*stacktop;
  int flags;
  int childpid;
  char scratch[2000];
  int fd;

  setbuf(stdout,NULL);
  if(socketpair(AF_UNIX,SOCK_STREAM,0,sockets)<0){
    perror("opening stream socket pair");
    exit(1);
  }
  stack= malloc(STACK_SIZE);
  if(stack==NULL){
    perror("child stack alloc failed");
    exit(1);
  }
  stacktop= stack+STACK_SIZE;

  flags= CLONE_NEWIPC|CLONE_NEWNS|CLONE_NEWPID|CLONE_NEWUSER|CLONE_NEWUTS|SIGCHLD;

  sharedenvp= envp;
  childpid= clone(child_code,stacktop,flags,argv+1);
  if(childpid==-1){
    perror("clone failed");
    exit(1);
  }

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

  write(sockets[1],"go",2);

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

int child_code(void*arglist){
  char okmsg[3];
  int ret;
  int i;
  char scratch[2000];

  char**args;
  char*rootdir;
  char*cmd;
  char**cmdargs;

  okmsg[2]= 0;
  read(sockets[0],okmsg,2);
  if(0){
    printf("Child got ok %s\n",okmsg);
  }


  args= (char**)arglist;
  rootdir= args[0];
  cmd= args[1];
  cmdargs= args+1;
  if(0){
    printf("rootdir = %s\n",rootdir);
    printf("cmd = %s\n",cmd);
  }

  if (MOUNT_QEMU == 1){
    sprintf(scratch,"%s/lib64",rootdir);
    domount(scratch,"/lib64");

    sprintf(scratch,"%s/lib/x86_64-linux-gnu",rootdir);
    domount(scratch,"/lib/x86_64-linux-gnu");

    if (0) {
    sprintf(scratch,"%s/usr",rootdir);
    ret= mkdir(scratch,0755);
    sprintf(scratch,"%s/usr/public",rootdir);
    ret= mkdir(scratch,0755);
    sprintf(scratch,"%s/usr/public/opt",rootdir);
    ret= mkdir(scratch,0555);
    ret= mount(PUBEXTERNALDIR,scratch,NULL,MS_MGC_VAL|MS_BIND|MS_REC|MS_RDONLY,NULL);
    if(ret!=0){
      perror("cask: Mount of /usr/public/opt failed - skipping");
    }
    }

    sprintf(scratch,"%s/qemu",rootdir);
    domount(scratch,QEMUEXTERNALDIR);
  }
  if(MOUNT_DEV){
    sprintf(scratch,"%s/dev",rootdir);
    domount(scratch,"/dev");
  }

  if(MOUNT_PROC){
    sprintf(scratch,"%s/proc",rootdir);
    domount(scratch,"/proc");
  }

  ret= chroot(rootdir);
  if(ret!=0){
    perror("cask: Chroot failed");
    exit(22);
  }

  ret= chdir("/");
  if(ret!=0){
    perror("cask: Chdir to / failed");
    exit(66);
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
  }
  if(0){
    printf("Access to %s for execute is %d\n",cmd,ret);
    printf("Calling %s with ",cmd);
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
