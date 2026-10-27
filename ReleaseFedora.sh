#!/bin/bash -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Load environment variables from .env file if exists
if [ -f ".env" ]; then
    set -a  # Automatically export all variables
    source .env
    set +a
fi

# Perform local build if QT_PREFIX is defined
if [ -n "$QT_PREFIX" ]; then
    rm -rf Release
    mkdir -p Release/Build
    cd Release/Build

    cmake ../../Sources -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DCMAKE_BUILD_TYPE=Release
    cmake --build . --target all deploy_rpm --parallel

    mv Output/* ..
    cd ../..

    rm -rf Release/Build
    exit 0
fi

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo "ERROR: Docker is not installed. Please install Docker and try again."
    exit 1
fi

IMAGE_NAME="drop-fedora-builder"

# Build Docker image if it doesn't exist
if ! docker image inspect "$IMAGE_NAME" &> /dev/null; then
    echo "Docker image '$IMAGE_NAME' not found. Building..."
    docker build -t "$IMAGE_NAME" -f Dockerfile.fedora .
fi

docker run --rm \
    -u "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -v "$SCRIPT_DIR:/workspace" \
    -w /workspace \
    "$IMAGE_NAME" \
    /bin/bash -c "./ReleaseFedora.sh"
