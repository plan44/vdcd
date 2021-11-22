FROM ubuntu:bionic as builder


ENV GIT_SRC https://github.com/plan44/vdcd.git
ENV BRANCH master
ENV DSROOT /app


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


RUN git clone ${GIT_SRC} ${DSROOR}/vdcd


RUN cd ${DSROOR}/vdcd && \
    git checkout ${BRANCH} && \
    git submodule init && \
    git submodule update

RUN cd ${DSROOR}/vdcd && \
    autoreconf -i && \
    ./configure && \
    make clean && make all

FROM ubuntu:bionic

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

COPY --from=builder ${DSROOR}/vdcd/vdcd /bin

CMD  ["/bin/vdcd"]
