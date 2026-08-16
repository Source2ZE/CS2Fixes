FROM ghcr.io/source2ze/build-containers:steamrt3

WORKDIR /app

RUN git clone https://github.com/alliedmodders/metamod-source
RUN git config --global --add safe.directory /app

COPY ./docker-entrypoint.sh ./
ENV HL2SDKCS2=/app/source/sdk
ENV MMSOURCE_DEV=/app/metamod-source
WORKDIR /app/source
CMD [ "/bin/bash", "./docker-entrypoint.sh" ]