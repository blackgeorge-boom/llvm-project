# syntax=docker/dockerfile:1

# stage 1
FROM compor/unasl-tools-base:latest as unasl-compiler-builder
LABEL maintainer "UnASL Developers"

ARG build_type=RelWithDebInfo

ENV DEBIAN_FRONTEND=noninteractive
ENV LLVM_DIR=llvm-project
ENV UNASL_DIR=UnASL
ENV TOOLS_DIR=tools

WORKDIR /

# update apt's source list and include the sources of the packages
RUN grep deb /etc/apt/sources.list | \
      sed 's/^deb/deb-src /g' >> /etc/apt/sources.list

RUN apt-get update && \
      apt-get install -y --no-install-recommends ca-certificates gnupg tzdata \
      build-essential python wget unzip nano ninja-build cmake git zip gdb \
      python3 openssh-client gcc-aarch64-linux-gnu g++-aarch64-linux-gnu && \
      rm -rf /var/lib/apt/lists/*

# setup git access to private repo using ssh certificate used as deploy key
RUN mkdir /root/.ssh && \
      touch /root/.ssh/known_hosts && \
      ssh-keyscan github.com >> /root/.ssh/known_hosts

RUN --mount=type=ssh git clone git@github.com:systems-nuts/UnASL.git ${UNASL_DIR}

# build and install custom llvm
RUN git clone https://github.com/blackgeorge-boom/llvm-project.git ${LLVM_DIR} && \
      git -C ${LLVM_DIR} checkout dev-unasl-docker && \
      cp ${LLVM_DIR}/llvm/utils/docker/scripts/build-unasl-llvm.sh . && \
      cp -r ${LLVM_DIR}/llvm/utils/docker/unasl/patches . && \
      git -C ${LLVM_DIR} checkout development && \
      cd ${LLVM_DIR} && patch -p1 < ../patches/llvmorg-9.0.1.patch && cd - && \
      mkdir build-llvm-unasl && \
      ./build-unasl-llvm.sh ${LLVM_DIR}/llvm ./build-llvm-unasl ${TOOLS_DIR}/toolchain-unasl ${build_type} && \
      ninja -C ./build-llvm-unasl install


## stage 2
FROM compor/unasl-tools-base:latest as unasl-compiler-base
LABEL maintainer "UnASL Developers"

ENV DEBIAN_FRONTEND=noninteractive
ENV UNASL_DIR=UnASL
ENV TOOLS_DIR=tools
ENV DEFS_MKFILE=../common/docker.defs.mk

WORKDIR /

# install packages for minimal useful image
RUN apt-get update && \
      apt-get install -y --no-install-recommends binutils libstdc++-11-dev \
      libstdc++-11-dev-arm64-cross \ ca-certificates gnupg build-essential \
      python wget unzip git nano ninja-build cmake zip gdb python3 tzdata \
      gcc-aarch64-linux-gnu g++-aarch64-linux-gnu && \
      rm -rf /var/lib/apt/lists/*

# copy build results of previous stage
COPY --from=unasl-compiler-builder ${TOOLS_DIR} ${TOOLS_DIR}
COPY --from=unasl-compiler-builder ${UNASL_DIR} ${UNASL_DIR}

# exercise aligned layout building on some programs
RUN make -C ${UNASL_DIR}/layout/bubble-sort && \
      make -C ${UNASL_DIR}/layout/call-leaf
