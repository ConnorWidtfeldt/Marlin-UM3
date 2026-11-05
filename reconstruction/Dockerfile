FROM amd64/ubuntu@sha256:efc44a68d205033006a2f924c85189d384615943d065cb255493bdc5f63060ad
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends cmake make curl ca-certificates python3 libgmp10 libmpc3 && rm -rf /var/lib/apt/lists/*
COPY toolchain.urls toolchain.sha256 /opt/um3-toolchain/
RUN mkdir /tmp/toolchain && cd /tmp/toolchain \
 && while read -r url; do curl --fail --location --retry 3 --remote-name "$url"; done < /opt/um3-toolchain/toolchain.urls \
 && sha256sum -c /opt/um3-toolchain/toolchain.sha256 \
 && dpkg -i libmpfr4_3.1.5-1_amd64.deb binutils-avr_2.24+Atmel3.4.4-1_amd64.deb gcc-avr_4.8.1+Atmel3.4.4-2_amd64.deb avr-libc_1.8.0+Atmel3.4.4-1_all.deb \
 && tar -xzf arduino-1.0.5-linux64.tgz -C /usr/share \
 && mv /usr/share/arduino-1.0.5 /usr/share/arduino \
 && rm -rf /tmp/toolchain
ENV PATH=/usr/bin:/bin:/usr/local/bin
ENV LC_ALL=C
WORKDIR /source
