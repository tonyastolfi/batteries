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
RUN apt-get update && apt-get install --no-install-recommends -y -q dialog apt-utils libgl1-mesa-glx

# Basic CLI tools.
#
RUN apt-get update && apt-get install --no-install-recommends -y nano sed locales

# Developer/Build tools.
#
RUN apt-get update && apt-get install --no-install-recommends -y git gcc g++ make cmake gdb

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

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Install the GitLab release cli
#
# Basic CLI tools.
#
RUN apt-get update && apt-get install --no-install-recommends -y curl

RUN curl --location --output /usr/local/bin/release-cli \
      "https://release-cli-downloads.s3.amazonaws.com/latest/release-cli-linux-amd64" && \
    chmod +x /usr/local/bin/release-cli && \
    release-cli -v
