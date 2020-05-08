FROM ubuntu:focal

ENV ANDROID_NDK_PATH /opt/android-ndk

RUN apt-get update \
 && DEBIAN_FRONTEND="noninteractive" apt-get install -y \
      adb \
      build-essential \
      curl \
      gcovr \
      git \
      lcov \
      libprocps-dev \
      libssl-dev \
      libuv1-dev \
      ninja-build \
      wget \
      xutils-dev \
      zlib1g-dev \
 && apt-get clean \
 \
 `# Skip the confirmation prompt the first time we try to clone a repo via ssh` \
 && mkdir -p ~/.ssh \
 && ssh-keyscan -H github.com >> ~/.ssh/known_hosts \
 \
 `# Install the Android NDK` \
 && mkdir -p /tmp/android-ndk \
 && cd /tmp/android-ndk \
 && wget -q http://dl.google.com/android/ndk/android-ndk-r10e-linux-x86_64.bin -O android-ndk.bin \
 && chmod a+x ./android-ndk.bin && sync && ./android-ndk.bin \
 && mv ./android-ndk-r10e /opt/android-ndk \
 && chmod -R a+rX /opt/android-ndk \
 && rm -rf /tmp/android-ndk \
 \
 `# Install CMake manually rather than via apt to get a new enough version` \
 && cd /opt \
 && wget -nv https://cmake.org/files/v3.17/cmake-3.17.0-Linux-x86_64.tar.gz \
 && tar zxf cmake-3.17.0-Linux-x86_64.tar.gz \
 && rm cmake-3.17.0-Linux-x86_64.tar.gz
