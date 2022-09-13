#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


/*
  compile as
  gcc -o forktest fork.c 
 */
int main(int argc, char **argv) {
  pid_t cpid,w;
  int status;
  printf("Main program %s started with %d args\n",argv[0],argc);

  cpid = fork();
  if (cpid == -1) {
    perror("fork failed");
    exit(-1);
  }

  if (cpid == 0) {
    /* Child */
    printf("Child PID %ld, parent PID %ld, uid %ld, euid %ld, gid %ld, egid %ld\n",
	   getpid(), getppid(), getuid(), geteuid(), getgid(), getegid());
  } else {
    /* Parent */
    do {
      w = waitpid(cpid,&status, WUNTRACED | WCONTINUED);
      if (w == -1) {
	perror("Waitpid failed");
	exit(-1);
      }
      if (WIFEXITED(status)) {
	printf("exited, status=%d\n", WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
	printf("killed by signal %d\n", WTERMSIG(status));
      } else if (WIFSTOPPED(status)) {
	printf("stopped by signal %d\n", WSTOPSIG(status));
      } else if (WIFCONTINUED(status)) {
	printf("continued\n");
      }
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }   
  return 0;
}
