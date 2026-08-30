#!/bin/sh
#
# The image entrypoint. The SDL OSD opens a video device whatever -video says
# and exits if it cannot, and a container has no display unless one is handed
# in, so default to the driver that needs none. The interactive run in the
# README passes DISPLAY through, and that is left for SDL to work out as usual.

if [ -z "${SDL_VIDEODRIVER:-}" ] &&
   [ -z "${DISPLAY:-}" ] &&
   [ -z "${WAYLAND_DISPLAY:-}" ]; then
	SDL_VIDEODRIVER=dummy
	export SDL_VIDEODRIVER
fi

exec /opt/rosco/rosco "$@"
