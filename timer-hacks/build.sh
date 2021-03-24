set -e

clang timer.c -O1 -o 'timer-firestorm'
clang timer.c -O1 -o 'timer-icestorm' -D ICESTORM=1

clang timer.c -O1 -arch arm64e -o 'timer-firestorm-arm64e'
clang timer.c -O1 -arch arm64e -o 'timer-icestorm-arm64e' -D ICESTORM=1