# syntax=docker/dockerfile:1

##=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++
# This Dockerfile builds a container image that can be used to build batteries
# on Ubuntu with GCC for CI/CD.
#
# Some useful commands:
#  docker login registry.gitlab.com
#  docker build -t registry.gitlab.com/tonyastolfi/batteries .
#  docker push registry.gitlab.com/tonyastolfi/batteries
#
FROM ubuntu:20.04

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Install development tools.
#

# Configure APT.
#
ENV TERM screen
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections
RUN apt-get update && apt-get install --no-install-recommends -y -q dialog apt-utils

# Basic CLI tools.
#
RUN apt-get update && apt-get install --no-install-recommends -y nano sed locales

# Developer/Build tools.
#
RUN apt-get update && apt-get install --no-install-recommends -y git gcc g++ make cmake

# Some libraries.
#
RUN apt-get update && apt-get install --no-install-recommends -y libc-dev libssl-dev libz-dev

# Conan.
#
RUN apt-get update && apt-get install --no-install-recommends -y python3 python3-pip
RUN pip3 install conan

# Clean up apt caches.
#
RUN rm -rf /var/lib/apt/lists/*

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Enable UTF-8 locale for rendering non-ASCII characters.
#
RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Configure Conan. 
#
RUN conan config init

# Use libstdc++11, c++17 by default; enable accurate stack traces.
#
RUN conan profile update 'settings.compiler.libcxx=libstdc++11' default
RUN conan profile update 'settings.compiler.cppstd=17' default
RUN conan profile update 'env.CXXFLAGS=-fno-omit-frame-pointer' default
RUN conan profile update 'env.CFLAGS=-fno-omit-frame-pointer' default
RUN conan profile update 'env.LDFLAGS=-fno-omit-frame-pointer' default

# Point at various release package repos.
#
RUN conan remote add gitlab https://gitlab.com/api/v4/packages/conan
RUN conan remote add mw-insidelabs-gitlab https://insidelabs-git.mathworks.com/api/v4/packages/conan

# Heuristic Optimization: pre-cache some commonly used packages.
#
RUN ( cd /tmp && rm -f conanfile.txt && \
      echo '[requires]' >conanfile.txt && \
      echo 'boost/1.77.0' >>conanfile.txt && \
      echo 'icu/69.1'  >>conanfile.txt && \
      echo 'gtest/cci.20210126'  >>conanfile.txt && \
      echo 'libcap/2.58'  >>conanfile.txt && \
      echo 'openssl/1.1.1l'  >>conanfile.txt && \
      echo 'zlib/1.2.12'  >>conanfile.txt && \
      echo 'protobuf/3.17.1'  >>conanfile.txt && \
      echo 'expat/2.4.4' >>conanfile.txt && \
      conan install . -s build_type=Release --build=missing )

# We have to do this multiple times to avoid version conflicts.
#
RUN ( cd /tmp && rm -f conanfile.txt && \
      echo '[requires]' >conanfile.txt && \
      echo 'boost/1.78.0' >>conanfile.txt && \
      echo 'icu/71.1' >>conanfile.txt && \
      echo 'expat/2.4.8' >>conanfile.txt && \
      echo 'protobuf/3.20.0' >>conanfile.txt && \
      echo 'openssl/3.0.2' >>conanfile.txt && \
      conan install . -s build_type=Release --build=missing )

RUN ( cd /tmp && rm -f conanfile.txt && \
      echo '[requires]' >conanfile.txt && \
      echo 'boost/1.79.0' >>conanfile.txt && \
      echo 'openssl/1.1.1n' >>conanfile.txt && \
      echo 'tbb/2020.3' >>conanfile.txt && \
      conan install . -s build_type=Release --build=missing )
