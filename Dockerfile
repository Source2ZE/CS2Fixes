FROM registry.gitlab.steamos.cloud/steamrt/sniper/sdk

WORKDIR /app

RUN apt update &&apt install -y git python3-pip
RUN git clone https://github.com/alliedmodders/ambuild
RUN pip install ./ambuild
RUN git clone https://github.com/alliedmodders/metamod-source
RUN git config --global --add safe.directory /app

COPY . ./source
COPY ./docker-entrypoint.sh ./
ENV HL2SDKCS2=/app/source/sdk
ENV METAMOD112=/app/metamod-source
WORKDIR /app/source
CMD [ "/bin/bash", "./docker-entrypoint.sh" ]