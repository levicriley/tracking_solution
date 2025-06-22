# ---------- 1. Base image --------------------------------------------------
FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build pkg-config git ca-certificates \
        python3 python3-pip python3-setuptools \
    && rm -rf /var/lib/apt/lists/*

# ---------- 2. Optional APT dependencies ----------------------------------
#    List one package per line in apt.txt (can be empty).
COPY apt.txt /tmp/apt.txt
RUN set -e; \
    if [ -s /tmp/apt.txt ]; then \
        apt-get update && \
        grep -Ev '^\s*#|^\s*$' /tmp/apt.txt | xargs apt-get install -y --no-install-recommends && \
        rm -rf /var/lib/apt/lists/*; \
    fi

# ---------- 3. Optional Python dependencies -------------------------------
COPY python.txt /tmp/python.txt
RUN if [ -s /tmp/python.txt ]; then \
        grep -Ev '^\s*#|^\s*$' /tmp/python.txt | xargs -r pip install --no-cache-dir ; \
    fi

# ---------- 4. Copy source & build C++ tracker ----------------------------
WORKDIR /app
COPY . /app
RUN cmake -S . -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr/local && \
    cmake --build build -j && \
    cmake --install build

# ---------- 5. copy defaults.ini -----------------------------------------
COPY defaults.ini /app/defaults.ini


# ---------- 6. Default entrypoint -----------------------------------------
# Expect same CLI you specified earlier
ENTRYPOINT ["tracking-solution"]
CMD ["--help"]
