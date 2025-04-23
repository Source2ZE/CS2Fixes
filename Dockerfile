FROM registry.gitlab.steamos.cloud/steamrt/sniper/sdk

WORKDIR /app

RUN apt update && apt install -y clang-16
RUN ln -sf /usr/bin/clang-16 /usr/bin/clang && ln -sf /usr/bin/clang++-16 /usr/bin/clang++
RUN git clone https://github.com/alliedmodders/ambuild
RUN cd ambuild && python setup.py install && cd ..
RUN git clone -b k/sourcehook_alternative https://github.com/alliedmodders/metamod-source
# no recursive submodules, steamrt3 doesn't support ssh checkout
RUN cd metamod-source && git submodule update --init && cd ..
RUN git config --global --add safe.directory /app

COPY ./docker-entrypoint.sh ./
ENV HL2SDKCS2=/app/source/sdk
ENV MMSOURCE112=/app/metamod-source
WORKDIR /app/source
CMD [ "/bin/bash", "./docker-entrypoint.sh" ]