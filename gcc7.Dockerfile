FROM gcc:7

RUN apt-get update \
    && apt-get -y install \
        libssl1.0-dev \
        libuv1-dev \
        ninja-build \
    && apt-get clean

# Install CMake
RUN cd /opt \
    && wget https://cmake.org/files/v3.9/cmake-3.9.6-Linux-x86_64.tar.gz \
    && tar zxvf cmake-3.9.6-Linux-x86_64.tar.gz \
    && rm -f cmake-3.9.6-Linux-x86_64.tar.gz

ENV PATH "$PATH:/opt/cmake-3.9.6-Linux-x86_64/bin"