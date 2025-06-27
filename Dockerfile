ARG DSROOT=/app
ARG BRANCH=main

FROM ubuntu:jammy AS builder

ARG DSROOT
ARG BRANCH

ENV GIT_SRC=https://github.com/plan44/vdcd.git
ENV BRANCH=${BRANCH}
ENV DSROOT=${DSROOT}


RUN apt update && apt install -y \
    git \
    automake \
    libtool \
    autoconf \
    g++ \
    make \
    libjson-c-dev \
    libsqlite3-dev \
    protobuf-c-compiler \
    libprotobuf-c-dev \
    libboost-dev \
    libi2c-dev \
    libssl-dev \
    libavahi-core-dev \
    libavahi-client-dev


RUN git clone ${GIT_SRC} ${DSROOT}/vdcd


RUN cd ${DSROOT}/vdcd && \
    git checkout ${BRANCH} && \
    git submodule init && \
    git submodule update --recursive

RUN cd ${DSROOT}/vdcd && \
    autoreconf -i && \
    ./configure && \
    make clean && make all


FROM ubuntu:jammy

ARG DSROOT

RUN apt update && apt install -y \
    git \
    libjson-c-dev \
    libsqlite3-dev \
    protobuf-c-compiler \
    libprotobuf-c-dev \
    libboost-dev \
    libi2c-dev \
    libssl-dev \
    libavahi-core-dev \
    libavahi-client-dev

COPY --from=builder ${DSROOT}/vdcd/vdcd /bin

CMD  ["/bin/vdcd"]
