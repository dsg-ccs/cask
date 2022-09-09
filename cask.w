\input references

\def\title{Cask: A Clone-Based Container}
\def\shorttitle{Cask}
\def\author{David S. Greenberg and Michael F. Bridgland}
\def\affil{Center for Computing Sciences}
\def\version{{\bf Draft Version 0.00}} 
\def\date{\today}
\font\authorfont=cmr12
\font\affilfont=cmti12
\font\subtitlefont=cmr7 scaled \magstep 3
\font\subttitlefont=cmtt10 scaled \magstep1
\def\topofcontents{\null\vfill
  \centerline{\titlefont\title}
  \medskip
  \centerline{\subtitlefont\subtitle}
  \vskip24pt
  \centerline{\authorfont\author}
  \vskip10pt
  \centerline{\affilfont\affil}
  \vskip10pt
  \centerline{\authorfont\date}
  \vskip24pt\def\shorttitle{\title}
}
\def\subtitle{}

@i identifiers.w

@*1Overview.  This is the source code for the program {\tt cask}, which
is a based on a program {\tt mkuidns} by Dan Ridge. {\bf Mystery:\/}
What is this program supposed to accomplish, and how is it to be
incorporated into emulation experiments?
@(cask.c@>=
#define _GNU_SOURCE
@<Include standard library header files@>@;
@<Define macros@>@;
@<Declare global variables@>@;
@<Declare functions@>@;
@<Define functions@>@;

@ The cloned child calls |child_code()|, which awaits the go-ahead
from the parent-only code below before performing its task.
@<Define functions@>=
int main(int argc, char**argv, char**envp)
{
  @<Declare |main| local variables@>@;
  @<Ensure that |stdout| is not buffered@>@;
  @<Set up |sockets| or |exit()|@>@;
  @<Create |stack| and |stacktop| or |exit()|@>@;
  @<Define |flags|@>@;
  @<Clone to get |childpid| or |exit()|@>@;
  @<Parent-only code@>@;
}

@ Stevens~[\refStevensMCMXCIII] (pp.~122--125) discusses stream buffering
and the use of |@!setbuf()|, which is declared in {\tt stdio.h}.  Passing
|NULL| as the second argument of |setbuf()| causes the stream output to
be unbuffered. The function must be called before any output is directed
to the stream.
@<Ensure that |stdout| is not buffered@>=
{
  setbuf(stdout,NULL);
}

@ This makes a socket pair to be used as a sequencer: the child waits for
its parent to set up. The function |@!perror()| is declared in {\tt stdio.h}.
The function |@!socketpair()| is declared in {\tt sys/socket.h}.
@.stdio.h@>
@.sys/socket.h@>
@<Set up |sockets| or |exit()|@>=
{
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
    perror("opening stream socket pair");
    exit(1);
  }
}

@ According to comments in the original source code, these pointers
refer to memory for child stack, which---as we expect to |execve()|
soon---need not be much.
@<Declare |main| local variables@>=
char *stack;  
char *stacktop;  

@ @<Define macros@>=
#define STACK_SIZE (1024*1024)

@ @<Create |stack| and |stacktop| or |exit()|@>=
{
  stack = malloc(STACK_SIZE);
  if (stack == NULL) {
    perror("child stack alloc failed");
    exit(1);
  }
  stacktop = stack + STACK_SIZE;  // stack grows down
}

@ @<Declare |main| local variables@>=
int flags;

@ The flag constant |@!CLONE_NEWIPC| indicates that the child process should be
created in a new IPC namespace [?].
%
The flag constant |@!CLONE_NEWNS| indicates that the child process
should be started in a new mount namespace; later, when the child
calls |mount()|, the effects will be limited to the child's mount namespace.
%
The flag constant |@!CLONE_NEWPID| indicates that the child process
should be created in a new PID namespace.  Thus the child's PID will
be~|1|, and th child will be the |init| process for that namespace.
%
The flag constant |@!CLONE_NEWUSER| is not documented in the manual
page for |clone()|; however, it is explained in the manual page
for~|user_namespaces|.
%
The flag constant |@!CLONE_NEWUTS| indicates that the child process
should be created in a new UTS namespace with duplicates of the
identifiers in the parent's UTS namespace.
%
The value of |flags| usually includes |@!SIGCHLD| as the number of the
termination signal to be sent to the parent when the child dies.
@<Define |flags|@>=
{
  flags = CLONE_NEWIPC | CLONE_NEWNS |CLONE_NEWPID | CLONE_NEWUSER
     | CLONE_NEWUTS | SIGCHLD;
}

@ @<Declare |main| local variables@>=
int childpid;

@ A function call of the form |@!clone(f, s, flags, a, ...)| attempts
to create a child process that shares some of its execution context
with its parent.  In particular, the child and parent will share file
descriptors, signal handlers, and address space.  Upon creation, the
child calls the function at address~|f| with the argument array
beginning at~|a|; the child terminates upon return from the function
call, and uses the value returned by the function as its exit value.

The parent process must provide a stack for the child before calling
|clone()|, and pass the address of the top of the child stack as the
second argument of |clone()|.
The declaration of |@!clone()| is in {\tt sched.h}.

@.sched.h@>
@<Clone to get |childpid| or |exit()|@>=
{
  sharedenvp = envp;
  childpid = clone(child_code, stacktop, flags, argv+1);
  if (childpid == -1) {
    perror("clone failed");
    exit(1);
  }
}

@*1Parent-Only Code.

@ @<Parent-only code@>=
{
  @<Parent: adjust child's UID map@>@;
  @<Parent: adjust child's {\tt setgroups}@>@;
  @<Parent: adjust child's GID map@>@;
  @<Parent: tell child to continue@>@;
  @<Parent: await child termination@>@;
} 

@ @<Declare |main| local variables@>=
char scratch[2000];

@ The function |open()| is declared in {\tt fcntl.h}.
The function |write()| is declared in {\tt unistd.h}.
The function |kill()| is declared in {\tt signal.h}.
The functions |printf()| and |sprintf()| are declared in {\tt stdio.h}.
@.fcntl.h@>
@.signal.h@>
@.unistd.h@>
@<Parent: adjust child's UID map@>=
{
  int fd;
  
  sprintf(scratch,"/proc/%d/uid_map",childpid);
  fd = open(scratch,O_WRONLY);
  if (fd == -1) {
    printf("unable to open uid map\n");
    kill(childpid,SIGTERM);
    exit(2);
  }
  sprintf(scratch,"0 %06d 1\n",getuid());
  write(fd,scratch,11);
  close(fd);
  if (0) {
    printf("Set child %d uid for %d to 0\n%s",
    childpid,getuid(),scratch);
  }
}

@ @<Parent: adjust child's {\tt setgroups}@>=
{
  int fd;
  
  sprintf(scratch,"/proc/%d/setgroups",childpid);
  fd = open(scratch,O_WRONLY);
  if (fd == -1) {
    printf("unable to open setgroups\n");
    kill(childpid,SIGTERM);
    exit(2);
  }
  write(fd,"deny\n",5);
  close(fd);
  if (0) {
    printf("Set groups %d to deny\n",childpid);
  }
}

@ @<Parent: adjust child's GID map@>=
{
  int fd;
  
  sprintf(scratch,"/proc/%d/gid_map",childpid);
  fd = open(scratch,O_WRONLY);
  if (fd == -1) {
    printf("unable to open gid map\n");
    kill(childpid,SIGTERM);
    exit(2);
  }
  sprintf(scratch,"0 %06d 1\n",getgid());
  write(fd,scratch,11);
  close(fd);
  if (0) {
    printf("Set child %d gid for %d to 0\n%s",
    childpid,getgid(),scratch);
  }
}

@ @<Parent: tell child to continue@>=
{
  write(sockets[1],"go",2);
}

@ The type |@!pid_t| is defined in {\tt sys/types.h}.
The function |@!waitpid()| is declared in {\tt sys/wait.h}.
@.sys/types.h@>
@.sys/wait.h@>
@<Parent: await child termination@>=
{
  pid_t cpid;
  int status;
  
  cpid = waitpid(childpid,&status,0);
  if (cpid <= 0) {
    perror("parent waitpid failed");
    exit(1);
  }
  if (WIFEXITED(status)) {
    if (status != 0) {
      printf("exited, status=%d\n", WEXITSTATUS(status));
    }
// else can be silent
  }
  else if (WIFSIGNALED(status)) {
    printf("killed by signal %d\n", WTERMSIG(status));
  }
  else if (WIFSTOPPED(status)) {
    printf("stopped by signal %d\n", WSTOPSIG(status));
  }
  else if (WIFCONTINUED(status)) {
    printf("continued\n");
  }
  if (0) {
    printf("Parent sees child %d complete with status %d\n",cpid,status);
  }
}

@ @<Declare global variables@>=
static char **sharedenvp;
static int sockets[2];

@*1Child-Only Code.

@ This was a macro; now it is constant; in the future, it should be
set via a command-line argument.
@<Declare global variables@>=
int mount_qemu = 1;

@ @<Declare functions@>=
int child_code (void *);

@ @<Define functions@>=
int child_code (void *arglist) {@/
  @<Declare |child_code| local variables@>@;
  @<Child: read go-ahead from parent@>@;
  @<Child: define |args|, |rootdir|, |cmd|, and |cmdargs|@>@;
  if (mount_qemu) {
    @<Child: mkdir and mount |"/lib64"| or |exit(11)|@>@;
    @<Child: mkdir and mount QEMU directory or |exit(12)|@>@;
  }
  @<Child: try to mount |"/dev"|@>@;
  @<Child: change root directory to |rootdir| or |exit(22)|@>@;
  @<Child: change directory to |"/"| or |exit(66)|@>@;
  @<Child: verify read access to |cmd| or |exit(-2)|@>@;
  @<Child: verify execute access to |cmd| or |exit(-2)|@>@;
  @<Child: print command arguments@>@;
  @<Child: execute |cmd| with |cmdargs| or |exit(-2)|@>@;
  return 15; // never get here!!
}

@ @<Declare |child_code| local variables@>=
  char okmsg[3];
  int ret;
  char scratch[2000];

@ @<Child: read go-ahead from parent@>=
{
  okmsg[2] = 0;
  read(sockets[0],okmsg,2);
  if (0) {
    printf("Child got ok %s\n",okmsg);
  }
}

@ @<Declare |child_code| local variables@>=
char **args;
char *rootdir;
char *cmd;
char** cmdargs;

@ @<Child: define |args|, |rootdir|, |cmd|, and |cmdargs|@>=
{
  args = (char **)arglist;
  rootdir = args[0];
  cmd = args[1];
  cmdargs = args+1;
  if (0) {
    printf("rootdir = %s\n",rootdir);
    printf("cmd = %s\n",cmd);
  }
}

@ The function |@!mkdir()|\dots
@<Child: mkdir and mount |"/lib64"| or |exit(11)|@>=
{
  int ret;
  sprintf(scratch,"%s/lib64",rootdir);
  ret = mkdir(scratch,0555);
  ret = mount("/lib64",scratch,NULL,MS_MGC_VAL|MS_BIND|MS_REC|MS_RDONLY,NULL);
  if (ret != 0) {
    perror("cask: Mount of lib64 failed");
    exit(11);
  }
}

@ What's going on here?
@<Child: mkdir and mount QEMU directory or |exit(12)|@>=
{
  int ret;

  sprintf(scratch,"%s/qemu",rootdir);
  ret = mkdir(scratch,0555);
  ret = mount("/u06/dsg/mountasqemu", scratch, NULL,
     MS_MGC_VAL|MS_BIND|MS_REC|MS_RDONLY, NULL);
  if (ret != 0) {
    perror("cask: Mount of qemu failed");
    exit(12);
  }
}

@ @<Child: try to mount |"/dev"|@>=
{
  int ret;

  sprintf(scratch,"%s/dev",rootdir);
  ret = mkdir(scratch,0555);
  ret = mount("/dev", scratch, NULL, MS_MGC_VAL|MS_BIND|MS_REC|MS_RDONLY,NULL);
  if (0) {
    fprintf(stderr,"Mount of dev returned %d\n",ret);
  }
  if (ret != 0) {
    perror("cask: Mount of dev failed");
  }
}

@ The function |@!chroot()| is declared in {\tt unistd.h}.
@.unistd.h@>
@<Child: change root directory to |rootdir| or |exit(22)|@>=
{
  int ret;

  ret = chroot(rootdir);
  if (ret != 0) {
    perror("cask: Chroot failed");
    exit(22);
  }
}

@ The function |@!chdir()| is declared in {\tt unistd.h}.
@.unistd.h@>
@<Child: change directory to |"/"| or |exit(66)|@>=
{
  int ret;

  ret = chdir("/");
  if (ret != 0) {
    perror("cask: Chdir to / failed");
    exit(66);
  }
}

@ The function |@!access()| is declared in {\tt unistd.h}.
@.unistd.h@>
@<Child: verify read access to |cmd| or |exit(-2)|@>=
{
  ret = access(cmd,F_OK);
  if (ret != 0) {
    perror("cask: cmd does not exist");
    exit(-2);
  }
}

@ @<Child: verify execute access to |cmd| or |exit(-2)|@>=
{
  ret = access(cmd,X_OK);
  if (ret != 0) {
    perror("cask: No execute access to cmd");
    exit(-2);
  }
  if (0) {
    printf("Access to %s for execute is %d\n",cmd,ret);
    printf("Calling %s with ",cmd);
  }
}

@ @<Child: print command arguments@>=
{
  int i;
  
  for (i = 0; i < 10; i++) {
    if (cmdargs[i] == NULL) break;
    printf(" %s",cmdargs[i]);
  }
  printf("\n");
}

@ The function |execve()| is declared in {\tt unistd.h}.
@.unistd.h@>
@<Child: execute |cmd| with |cmdargs| or |exit(-2)|@>=
{
  ret = execve(cmd,cmdargs,sharedenvp);
  if (ret != 0) {
    perror("cask: Execve failed");
    exit(-2);
  }
}

@
@.errno.h@>
@.fcntl.h@>
@.sched.h@>
@.signal.h@>
@.stdio.h@>
@.stdlib.h@>
@.string.h@>
@.sys/mount.h@>
@.sys/socket.h@>
@.sys/stat.h@>
@.sys/types.h@>
@.sys/wait.h@>
@.unistd.h@>
@<Include standard library header files@>=
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

@i chapter-references.w

@*1Index.
