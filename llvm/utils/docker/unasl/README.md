# Docker image builds for UnASL

## Introduction

The docker images for UnASL are split in two parts:

1) Base tools image: This sets up the base tools required by UnASL which currently are:
  - LLVM/Clang "vanilla" toolchain: with no custom changes
  - LLVM/Clang custom toolchain for musl libc: required version to build the UnASL musl libc
  - Popcorn compiler: required for stackmaps and related functionality
  - musl libc: required for static binary building


2) UnASL compiler image: This sets up the UnASL compiler along with supporting repository for its Make invocation.  
   This includes:
   - LLVM/Clang custom UnASL toolchain: the UnASL compiler to generate stack aligned binaries for x86-64 and AArch64
   - UnASL repository: containing Make tools, scripts and benchmarks


Both images use a [multi-stage][1] build approach, with 2 stages each.
The first image is used for building the tools and holds all the by-products of compilation,
while the second is a slim version that copies over only the final build artifacts.

The image from the second stage is the one that published.
The UnASL compiler image builds upon the base tools image.

Currently, the images are hosted on Docker Hub in 2 repositories:

1) compor/unasl-tools-base
2) compor/unasl-compiler

The images are built with multi-arch support (i.e., linux/amd64 and linux/arm64).

## Requirements

We build the Docker images using [BuildKit][2] which set the minimum required Docker engine version to 18.06.
BuildKit is used because it makes it easier to build multi-arch images with the [`buildx`][3] cli tool.

Moreover, since we are using a private Github repository, we also use BuildKit because it allows to
access private data using SSH authentication as described [here][4].

## How to build

To setup Docker for multi-arch support you might need to follow the instructions described [here][5].

After that, you can setup a custom builder:

```
docker buildx create --name unaslbuilder --driver-opt network=host --platform linux/arm64,linux/amd64 --use
docker buildx inspect --bootstrap
```

Using that builder, building and pushing images can be done as:

```
docker buildx build . --platform linux/amd64,linux/arm64 --tag compor/unasl-tools-base:latest --push
```

in each corresponding directory per image.

If the build requires access to private repositories, add the option:

```
--ssh default=${SSH_AUTH_SOCK}
```

assuming that `ssh-agent` is running, its standard environment is load (e.g., by executing `$(eval ssh-agent -s)`
and the appropriate keys have been adding (e.g., by using `ssh-add`).


## Other

Using SSH keys as deploy keys for read-only access to private Github repositories
as required by Docker is documented [here][6].

Respectively, access token to Docker Hub for pushing images is documented [here][7].


[1]: https://docs.docker.com/develop/develop-images/multistage-build/
[2]: https://github.com/moby/buildkit
[3]: https://docs.docker.com/buildx/working-with-buildx/
[4]: https://docs.docker.com/develop/develop-images/build_enhancements/#using-ssh-to-access-private-data-in-builds
[5]: https://docs.docker.com/buildx/working-with-buildx/#build-multi-platform-images
[6]: https://docs.github.com/en/developers/overview/managing-deploy-keys
[7]: https://docs.docker.com/docker-hub/access-tokens/
