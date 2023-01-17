FROM ubuntu:22.04 as base

RUN apt-get update \
    && apt-get install ca-certificates -y \
    && update-ca-certificates \
    && sed -i "s@http://.*archive.ubuntu.com@https://mirrors.tuna.tsinghua.edu.cn@g" /etc/apt/sources.list \
    && sed -i "s@http://.*security.ubuntu.com@https://mirrors.tuna.tsinghua.edu.cn@g" /etc/apt/sources.list \
    && apt-get update \
    && apt-get install apt-utils -y \    
    && apt-get install vim -y \    
    && apt-get install bison -y \    
    && apt-get install flex -y \    
    && apt-get install build-essential -y \    
    && apt-get install man -y \
    && apt-get install gcc-doc -y \
    && apt-get install gdb -y \
    && apt-get install git -y \
    && apt-get install libreadline-dev -y \
    && apt-get install libsdl2-dev -y \
    && apt-get install llvm -y

