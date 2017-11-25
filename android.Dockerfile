FROM ubuntu:xenial

RUN apt-get update && \
    apt-get install -y wget build-essential lcov curl gcovr libssl-dev \
      git libuv1-dev ninja-build adb xutils-dev

# Install the Android NDK
RUN mkdir -p /tmp/android-ndk && \
    cd /tmp/android-ndk && \
    wget -q http://dl.google.com/android/ndk/android-ndk-r10e-linux-x86_64.bin -O android-ndk.bin && \
    chmod a+x ./android-ndk.bin && sync && ./android-ndk.bin && \
    mv ./android-ndk-r10e /opt/android-ndk && \
    chmod -R a+rX /opt/android-ndk && \
    rm -rf /tmp/android-ndk

ENV ANDROID_NDK /opt/android-ndk

# Install CMake
RUN cd /opt \
    && wget https://cmake.org/files/v3.9/cmake-3.9.6-Linux-x86_64.tar.gz \
    && tar zxvf cmake-3.9.6-Linux-x86_64.tar.gz \
    && rm -f cmake-3.9.6-Linux-x86_64.tar.gz

ENV PATH "$PATH:/opt/cmake-3.9.6-Linux-x86_64/bin"
