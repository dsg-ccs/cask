testroot:
	-mkdir testroot

testprogs:     testroot
	gcc -o testroot/forktest-dyn  fork.c
	gcc -o testroot/forktest --static fork.c

