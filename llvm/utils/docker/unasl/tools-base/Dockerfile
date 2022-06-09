# syntax=docker/dockerfile:1

# stage 1
FROM ubuntu:21.10 as tools-base-builder
LABEL maintainer "UnASL Developers"

ENV DEBIAN_FRONTEND=noninteractive
ENV LLVM_DIR=llvm-project
ENV MUSL_DIR=musl-stack-reloc
ENV POPCORN_DIR=popcorn-compiler
ENV TOOLS_DIR=tools

WORKDIR /

# update apt's source list and include the sources of the packages
RUN grep deb /etc/apt/sources.list | \
      sed 's/^deb/deb-src /g' >> /etc/apt/sources.list

RUN apt-get update && \
      apt-get install -y --no-install-recommends ca-certificates gnupg tzdata \
      build-essential python wget subversion unzip zip nano ninja-build git \
      flex bison texinfo gdb python3 cmake binutils \
      gcc-x86-64-linux-gnu g++-x86-64-linux-gnu \
      gcc-aarch64-linux-gnu g++-aarch64-linux-gnu && \
      rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/blackgeorge-boom/llvm-project.git ${LLVM_DIR}

RUN git -C ${LLVM_DIR} checkout dev-unasl-docker && \
      cp ${LLVM_DIR}/llvm/utils/docker/scripts/build-unasl-llvm.sh . && \
      cp -r ${LLVM_DIR}/llvm/utils/docker/unasl/patches . && \
      git -C ${LLVM_DIR} checkout llvmorg-9.0.1 && \
      cd ${LLVM_DIR} && patch -p1 < ../patches/llvmorg-9.0.1.patch

# build and install vanilla LLVM toolchain
RUN mkdir ./build-llvm-vanilla && \
      ./build-unasl-llvm.sh ${LLVM_DIR}/llvm ./build-llvm-vanilla ${TOOLS_DIR}/toolchain-vanilla && \
      ninja -C ./build-llvm-vanilla install

# build and install LLVM toolchain required for musl libc build
RUN mkdir ./build-llvm-musl && \
      cd ${LLVM_DIR} && git restore * && cd - && \
      git -C ${LLVM_DIR} checkout c6780392ecf34ebf67b011eac483ff6a1fd38dfc && \
      cd ${LLVM_DIR} && patch -p1 < ../patches/llvmorg-9.0.1.patch && cd - && \
      ./build-unasl-llvm.sh ${LLVM_DIR}/llvm ./build-llvm-musl ${TOOLS_DIR}/toolchain-musl && \
      ninja -C ./build-llvm-musl install

# build and install musl libc
RUN git clone https://github.com/systems-nuts/musl-stack-reloc.git ${MUSL_DIR} && \
      git -C ${MUSL_DIR} checkout unasl && \
      ${MUSL_DIR}/install_musl.sh -d ${TOOLS_DIR}/musl -c ${TOOLS_DIR}/toolchain-musl/

# build and install Popcorn compiler
RUN git clone https://github.com/compor/popcorn-compiler.git ${POPCORN_DIR} && \
      git -C ${POPCORN_DIR} checkout dev-install-fixes && \
      ${POPCORN_DIR}/install_compiler.py --install-all --threads $(nproc) --base-path ${POPCORN_DIR}


## stage 2
FROM ubuntu:21.10 as tools-base
LABEL maintainer "UnASL Developers"

ENV DEBIAN_FRONTEND=noninteractive
ENV TOOLS_DIR=tools

WORKDIR /

# install packages for minimal useful image
RUN apt-get update && \
      apt-get install -y --no-install-recommends binutils flex bison tzdata \
      texinfo gdb python3 libstdc++-11-dev libstdc++-11-dev-arm64-cross python \
      build-essential wget unzip zip ca-certificates gnupg \
      gcc-x86-64-linux-gnu g++-x86-64-linux-gnu \
      gcc-aarch64-linux-gnu g++-aarch64-linux-gnu && \
      rm -rf /var/lib/apt/lists/*

# copy build results of previous stage
COPY --from=tools-base-builder ${TOOLS_DIR} ${TOOLS_DIR}
COPY --from=tools-base-builder /usr/local/popcorn /usr/local/popcorn

