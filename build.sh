#!/usr/bin/env bash
# METABUILD - (c) 2025 FRINKnet & Friends - 0BSD

set -euo pipefail

# THIS REPO
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

# NOW USE IT
if [ "$1" = "cli" ]; then
  shift
  exec docker run --rm -it -u $(id -u):$(id -g) -v "\$(pwd):/build" --entrypoint bash $IMAGE "\$@"
else
  exec docker run --rm -it -u $(id -u):$(id -g) -v "\$(pwd):/build" $IMAGE "$@"
fi
