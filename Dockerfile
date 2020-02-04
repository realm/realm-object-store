FROM ubuntu:xenial

RUN apt-get update && \
    apt-get install -y wget build-essential lcov curl cmake gcovr libprocps4-dev libssl-dev \
      git python-cheetah libuv1-dev ninja-build adb xutils-dev unzip

# Install the Android NDK
RUN mkdir -p /tmp/android-ndk && \
    cd /tmp/android-ndk && \
    wget -q https://dl.google.com/android/repository/android-ndk-r21-linux-x86_64.zip -O android-ndk.zip && \
    unzip ./android-ndk.zip && \
    mv ./android-ndk-r21 /opt/android-ndk && \
    chmod -R a+rX /opt/android-ndk && \
    rm -rf /tmp/android-ndk

ENV ANDROID_NDK_PATH /opt/android-ndk

# Ensure a new enough version of CMake is available.
RUN cd /opt \
    && wget https://cmake.org/files/v3.15/cmake-3.15.2-Linux-x86_64.tar.gz \
        && tar zxvf cmake-3.15.2-Linux-x86_64.tar.gz

ENV PATH "/opt/cmake-3.15.2-Linux-x86_64/bin:$PATH"
