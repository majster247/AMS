#!/bin/sh

unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     machine=Linux;;
    Darwin*)    machine=Mac;;
    CYGWIN*)    machine=Cygwin;;
    MINGW*)     machine=MinGw;;
    MSYS_NT*)   machine=Git;;
    *)          machine="UNKNOWN:${unameOut}"
esac



if [[ "$machine" == Mac ]]; then

    #Setting up brew if not installed to install neccesary commands
    if ! command -v wget &> /dev/null
    then
        echo "Wget could not be found, installing it via brew"
        if ! command -v brew &> /dev/null
        then
            git clone https://github.com/Homebrew/brew homebrew
            eval "$(homebrew/bin/brew shellenv)"
            brew update --force --quiet
            chmod -R go-w "$(brew --prefix)/share/zsh"
            brew install wget
        else
            brew install wget
        fi
    fi

    #Downloading sources
    echo "Downloading Binutils"
    wget https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.gz

    echo "Downloading GCC"
    wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz

    export CC=/usr/bin/gcc-4.2
    export CXX=/usr/bin/g++-4.2
    export CPP=/usr/bin/cpp-4.2
    export LD=/usr/bin/gcc-4.2
    echo "Machine kernel detected is: "$machine {unameOut}
    echo "Initializing building a ENV to compile system"

    wget https://github.com/macports/macports-base/releases/download/v2.8.1/MacPorts-2.8.1.tar.gz
    tar xzvf MacPorts-2.8.1.tar.gz
    cd MacPorts-2.8.1
    ./configure && make && sudo make install

    mkdir build-binutils
    mkdir build-gcc

    tar -xf gcc-13.2.0.tar.gz
    tar -xf binutils-2.41.tar.gz

    cd build-binutils


fi

