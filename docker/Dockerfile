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
RUN apt-get update
RUN apt-get install -y -q dialog apt-utils

# Basic CLI tools.
#
RUN apt-get install -y nano
RUN apt-get install -y sed
RUN apt-get install -y locales

# Developer/Build tools.
#
RUN apt-get install -y git
RUN apt-get install -y gcc
RUN apt-get install -y make
RUN apt-get install -y cmake

# Some libraries.
#
RUN apt-get install -y libssl-dev
RUN apt-get install -y libz-dev

# Conan.
#
RUN apt-get install -y python3
RUN apt-get install -y python3-pip
RUN pip3 install conan

# Enable UTF-8 locale for rendering chess piece glyphs.
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
