#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=${1:-"$root/artifacts/stock-comparison"}
mkdir -p "$output"
output=$(CDPATH= cd -- "$output" && pwd)
image=homefab-um3-stock:gcc4.8.1-atmel3.4.4
docker build --platform linux/amd64 --tag "$image" --file "$root/reconstruction/Dockerfile" "$root/reconstruction"
image_id=$(docker image inspect "$image" --format '{{.Id}}')
# Match the stock image build strings; source identity is recorded separately.
epoch=${SOURCE_DATE_EPOCH:-1548629527}
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
