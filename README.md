# cask

The program cask is literate-programming version of an old suggestion from Dan Ridge to use clone(2) to build a simple container.

## Getting started

cd src
make

## Usage

See doc file in src directory

## Testing

pushd test
make testprogs
popd

src/cask test/testroot/ forktest
