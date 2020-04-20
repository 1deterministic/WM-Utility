#!/bin/bash

echo "Compiled binaries will be placed on $HOME/.local/bin"
echo "Remember to add $HOME/.local/bin to your PATH"

if [[ ! -d $HOME/.local ]]; then
    mkdir $HOME/.local
fi

if [[ ! -d $HOME/.local/bin ]]; then
    mkdir $HOME/.local/bin
fi

gcc -O3 ./audio/audio.c -o $HOME/.local/bin/audio -lm
gcc -O3 ./color/color.c -o $HOME/.local/bin/color -lm
gcc -O3 ./daemon/daemon.c -o $HOME/.local/bin/daemon -lm -lpthread
