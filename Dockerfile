# syntax=docker/dockerfile:1

# rosco emulator - see README.md
#
#   docker build -t rosco-emulator .
#   docker run --rm --entrypoint rosco-smoke-test rosco-emulator
#
# The builder needs the full MAME build dependencies; the runtime image keeps
# only the shared libraries the binary actually links against. Qt is left out
# of both - USE_QTDEBUG=0 gives the SDL debugger instead and drops Qt6Core,
# Gui, Widgets and DBus from the runtime.
#
# 22.04 is the oldest base the tree builds on, and the binary that comes out of
# it also runs on newer distributions - the other way round it would not. Both
# stages take the same base so the runtime libraries match what was linked.

ARG BUILDER_BASE=ubuntu:22.04
ARG RUNTIME_BASE=ubuntu:22.04

FROM ${BUILDER_BASE} AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --no-install-recommends -y \
		build-essential \
		python3 \
		pkg-config \
		libsdl2-dev \
		libsdl2-ttf-dev \
		libfontconfig-dev \
		libx11-dev \
		libxinerama-dev \
		libxext-dev \
		libxi-dev \
		libgl-dev \
		libasound2-dev \
		libpulse-dev \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# SYMBOLS=0 keeps the 24MB of DWARF out of the image, and REGENIE=1 because the
# makefile does not notice any of these options changing on their own.
RUN make -j"$(nproc)" REGENIE=1 USE_QTDEBUG=0 SYMBOLS=0 STRIP_SYMBOLS=1 \
	&& strip -s rosco

# A stage holding nothing but the binary, so the release build can pull it out
# with --target export --output type=local.
FROM scratch AS export
COPY --from=builder /src/rosco /rosco

FROM ${RUNTIME_BASE} AS runtime

ENV DEBIAN_FRONTEND=noninteractive
# libasound2 was renamed libasound2t64 in 24.04, so try the new name first and
# fall back for the older bases.
RUN apt-get update && apt-get install --no-install-recommends -y \
		libsdl2-2.0-0 \
		libsdl2-ttf-2.0-0 \
		libfontconfig1 \
		libx11-6 \
		libxinerama1 \
		libxext6 \
		libxi6 \
		libgl1 \
		libpulse0 \
		fonts-dejavu-core \
	&& (apt-get install --no-install-recommends -y libasound2t64 \
		|| apt-get install --no-install-recommends -y libasound2) \
	&& rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/rosco			/opt/rosco/rosco
COPY --from=builder /src/scripts/rosco-test.sh	/opt/rosco/scripts/rosco-test.sh
COPY --from=builder /src/scripts/smoke-test.sh	/opt/rosco/scripts/smoke-test.sh
COPY --from=builder /src/roms			/opt/rosco/roms
COPY --from=builder /src/bgfx			/opt/rosco/bgfx
COPY --from=builder /src/COPYING		/opt/rosco/COPYING
COPY --from=builder /src/README.md		/opt/rosco/README.md

RUN ln -s /opt/rosco/rosco /usr/local/bin/rosco \
	&& ln -s /opt/rosco/scripts/rosco-test.sh /usr/local/bin/rosco-test \
	&& ln -s /opt/rosco/scripts/smoke-test.sh /usr/local/bin/rosco-smoke-test

# uid 1000 is taken by the "ubuntu" account on 24.04 and later.
RUN userdel --remove ubuntu 2>/dev/null || true; \
	useradd --uid 1000 --create-home --home-dir /home/rosco --shell /bin/bash rosco \
	&& install -d -o rosco -g rosco /work

# The emulator reads $HOME/.rosco/rosco.ini whatever the working directory is,
# which is the only way the shipped ROMs stay findable once someone mounts
# their own directory over /work. A ROM directory in the working directory
# still wins, and -rompath on the command line overrides both.
#
# The rosco machines have neither sound nor MIDI hardware, and with the sound
# and MIDI modules left on, every container run opens with ALSA complaining
# about the /dev/snd that is not there. Pass -sound sdl to get it back.
RUN install -d -o rosco -g rosco /home/rosco/.rosco \
	&& printf '%s\n' \
		'rompath                   roms;/opt/rosco/roms' \
		'bgfx_path                 /opt/rosco/bgfx' \
		'sound                     none' \
		'midiprovider              none' \
		> /home/rosco/.rosco/rosco.ini \
	&& chown rosco:rosco /home/rosco/.rosco/rosco.ini

ENV HOME=/home/rosco
USER rosco
WORKDIR /work

LABEL org.opencontainers.image.title="rosco-emulator" \
      org.opencontainers.image.description="Emulator for the rosco_m68k and rosco_6502 single board computers, built as a slimmed-down MAME target" \
      org.opencontainers.image.source="https://github.com/solderdemon/rosco-emulator" \
      org.opencontainers.image.licenses="GPL-2.0-or-later"

ENTRYPOINT ["/opt/rosco/rosco"]
CMD ["-help"]
