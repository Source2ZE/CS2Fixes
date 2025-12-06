FROM registry.gitlab.steamos.cloud/steamrt/sniper/sdk

WORKDIR /app

RUN echo "deb http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-21 main" >> /etc/apt/sources.list.d/llvm.list
RUN echo "deb-src http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-21 main" >> /etc/apt/sources.list.d/llvm.list
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
RUN apt update && apt install -y clang-21
RUN ln -sf /usr/bin/clang-21 /usr/bin/clang && ln -sf /usr/bin/clang++-21 /usr/bin/clang++
RUN git clone https://github.com/alliedmodders/ambuild
RUN cd ambuild && python setup.py install && cd ..
RUN git clone https://github.com/alliedmodders/metamod-source
RUN git config --global --add safe.directory /app

COPY ./docker-entrypoint.sh ./
ENV HL2SDKCS2=/app/source/sdk
ENV MMSOURCE_DEV=/app/metamod-source
WORKDIR /app/source
CMD [ "/bin/bash", "./docker-entrypoint.sh" ]