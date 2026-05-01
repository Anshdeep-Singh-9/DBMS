FROM debian:bookworm-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential \
       ca-certificates \
       curl \
       g++ \
       make \
       tar \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

#
# What:
# Build both the terminal binary and the API server inside the image.
#
# Why:
# The project has two useful entrypoints:
# - `miniDB` for interactive CLI usage
# - `server` for HTTP/API usage
#
# Understanding:
# `make api` already pulls the Crow/Asio headers if missing and also builds the
# core project objects, so one command is enough for the packaged image.
#
# Concept used:
# multi-stage Docker build
#
# Layman version:
# first stage me hum project compile karte hain, second stage me sirf ready
# binaries le jaate hain so final image chhoti aur clean rahe.
#
RUN make api

FROM debian:bookworm-slim AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV MINIDB_HOME=/app

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/bin/miniDB /app/miniDB
COPY --from=builder /app/build/bin/server /app/server
COPY --from=builder /app/include /app/include
COPY --from=builder /app/api /app/api
COPY --from=builder /app/docker-entrypoint.sh /app/docker-entrypoint.sh
COPY --from=builder /app/README.md /app/README.md

RUN chmod +x /app/miniDB /app/server /app/docker-entrypoint.sh \
    && mkdir -p /app/table /app/system

VOLUME ["/app/table", "/app/system"]

EXPOSE 18080

ENTRYPOINT ["sh", "/app/docker-entrypoint.sh"]
CMD ["./server"]
