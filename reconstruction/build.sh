#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=${1:-"$root/artifacts/stock-comparison"}
mkdir -p "$output"
output=$(CDPATH= cd -- "$output" && pwd)
image=homefab-um3-stock:arduino-1.0.5
docker build --platform linux/amd64 --tag "$image" --file "$root/reconstruction/Dockerfile" "$root/reconstruction"
image_id=$(docker image inspect "$image" --format '{{.Id}}')
epoch=$(git -C "$root" log -1 --format=%ct)
commit=$(git -C "$root" rev-parse HEAD)
dirty=false
if test -n "$(git -C "$root" status --porcelain)"; then dirty=true; fi
docker run --rm --platform linux/amd64 --network none --read-only \
  --cap-drop ALL --security-opt no-new-privileges --user "$(id -u):$(id -g)" \
  --tmpfs /tmp:rw,exec,nosuid,nodev \
  --mount "type=bind,source=$root,target=/source,readonly" \
  --mount "type=bind,source=$output,target=/out" \
  --env SOURCE_DATE_EPOCH="$epoch" --env UM3_SOURCE_COMMIT="$commit" \
  --env UM3_SOURCE_DIRTY="$dirty" --env UM3_IMAGE_ID="$image_id" "$image" sh /source/reconstruction/build-in-container.sh
