#!/usr/bin/env bash
# METABUILD - (c) 2025 FRINKnet & Friends - 0BSD

set -eo pipefail

# THIS IS THE BUILD REPO
REPO="ghcr.io/frinknet/metabuild"
IMAGE="${REPO##*/}"

# MAKE SURE WE HAVE A CONTAINER
if ! docker image inspect "$IMAGE" &>/dev/null; then
  if ! docker pull "$REPO":latest; then
    echo ">>> Building Docker Image: $IMAGE"
    docker build -t "$IMAGE" .
  else
    docker tag "$REPO:latest" "$IMAGE"
  fi
fi

# NOW USE IT...
exec docker run --rm -it \
  -u $(id -u):$(id -g) \
  -e PRJ="${PWD##*/}" \
  -v "$PWD:/build" $IMAGE "$@"
