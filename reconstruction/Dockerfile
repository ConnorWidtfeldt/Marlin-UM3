FROM amd64/ubuntu@sha256:efc44a68d205033006a2f924c85189d384615943d065cb255493bdc5f63060ad
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends cmake make curl ca-certificates python3 && rm -rf /var/lib/apt/lists/*
RUN curl --fail --location --retry 3 https://downloads.arduino.cc/arduino-1.0.5-linux64.tgz -o /tmp/arduino.tgz \
 && echo '3047aa2a0482aea9eeafbea0c0a43756c8bda0772aaeff97856e1d70ad16cfaf  /tmp/arduino.tgz' | sha256sum -c - \
 && tar -xzf /tmp/arduino.tgz -C /usr/share \
 && mv /usr/share/arduino-1.0.5 /usr/share/arduino \
 && rm /tmp/arduino.tgz
ENV PATH=/usr/share/arduino/hardware/tools/avr/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ENV LC_ALL=C
WORKDIR /source
