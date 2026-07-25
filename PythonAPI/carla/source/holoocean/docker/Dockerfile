FROM adamrehn/ue4-runtime:22.04-cudagl11-noaudio

USER root
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2004/x86_64/3bf863cc.pub
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl git libglib2.0-dev software-properties-common

# OpenCV's runtime dependencies (and other dependencies)
RUN apt-get install -y libglib2.0-0 libsm6 libxrender-dev libxext6

# Install all python versions to test on
RUN apt-get update && apt-get install -y python3-dev python3-pip
RUN pip3 install setuptools wheel tox posix_ipc numpy

# Setup user
USER ue4

WORKDIR /home/ue4/source/holoocean/

# This should be COPY ../ but docker doesn't allow copying files outside the context
# To copy the project files either run the build command in this directory with the
# previous directory as the context: docker build -t frostlab/holoocean[:tag] -f ./Dockerfile ..
# or run it from the parent directory and provide the docker file location
# docker build -t frostlab/holoocean[:tag] -f ./docker/Dockerfile .
COPY --chown=ue4 ./ .
RUN pip3 install .

CMD ["/bin/bash"]