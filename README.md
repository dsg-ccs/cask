# cask

The program cask was literate-programming version of an old suggestion from DSKR to use clone(2) to
build a simple container.
The literate programming part has since been mostly abandonned so oonly the cask.c file is up to
date and using the cask.w will clobber the latest version.


## Getting started

cd src
make

## Usage

cask <rootdir> <curdir> <prog>
will run prog as if curdir is the current directory and the file system root is rootdir

Beware, for better or worse the root directory is your root directory and is where all dynamic
libraries are expected to be. Thus if your prog is dynamically linked then you are doomed to fail
unless you pick a root directory containing the libraries in the expected places.

The /proc and /dev directories will also not be, by default, in your root directory.
cask provides a variety of environment variables to allow for some work arounds
(yes there are better/more general ways to pass in info but this was simple)

setting MOUNT_PROC and/or MOUNT_DEV to 1 will cause cask to mount these external (host) directories
into your guest root file system. This is handy if you want /proc/random or /dev/null.

cask was designed for use in tandem with QEMU user mode.  Thus guest programs can use a guest CPU
architecture and run the desired program inside qemu user-mode.  This does require a full linux file
system for the guest architecture.  It also makes it difficult for the x86 (or host) version of QEMU
to run.  Thus cask allows setting CASK_QEMUDIR to a directory holding qemu user mode executables.
But these executables tend to be dynamically link so cask also allows the x86 library directories to
be added to the guest file system.


## Design

See .tex files in src directory for details of early design.
cask can attempt to create some mount points ini

## Testing

You will need the static version of the c libraries for this
pushd test
make testprogs
popd

src/cask test/testroot/ / forktest
