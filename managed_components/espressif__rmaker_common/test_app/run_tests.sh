#!/bin/bash

# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

# Run ESP RainMaker Common Component Tests on Hardware

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Parse command line arguments
FLASH_DEVICE=true
TEST_GROUP=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-flash)
            FLASH_DEVICE=false
            shift
            ;;
        work_queue|utils|cmd_resp|all)
            TEST_GROUP="$1"
            shift
            ;;
        *)
            TEST_GROUP="$1"
            shift
            ;;
    esac
done

# Default port - user can override with PORT environment variable
ESP_PORT=${PORT:-"/dev/tty.usbserial-110"}

echo "Building ESP RainMaker Common Test App..."
cd "$SCRIPT_DIR"

# Check if ESP-IDF environment is available
if ! command -v idf.py &> /dev/null; then
    echo "ERROR: ESP-IDF environment not found!"
    echo "Please run: source \$IDF_PATH/export.sh"
    echo "Or run this from an ESP-IDF terminal where idf.py is available"
    exit 1
fi

# Build the test app
idf.py build

# Flash the device (unless --no-flash specified)
if [ "$FLASH_DEVICE" = true ]; then
    echo "Flashing device..."
    idf.py -p "$ESP_PORT" flash
else
    echo "Skipping flash (--no-flash specified)"
fi

echo "Running hardware tests with pytest-embedded..."
echo "ESP32C3 device connected on port: $ESP_PORT"
echo ""

# Check if pytest-embedded is installed
if ! python -c "import pytest_embedded" 2>/dev/null; then
    echo "Installing pytest-embedded dependencies..."
    pip install pytest-embedded[serial]
fi

# Set pytest flash flag based on our --no-flash option
if [ "$FLASH_DEVICE" = true ]; then
    PYTEST_FLASH_FLAG=""
else
    PYTEST_FLASH_FLAG="--skip-autoflash"
fi

# Run tests using pytest.ini configuration (like ESP-Insights)
case "$TEST_GROUP" in
    work_queue)
        echo "Running work queue tests on hardware..."
        pytest --target=esp32c3 --port="$ESP_PORT" $PYTEST_FLASH_FLAG pytest_rmaker_common.py::test_work_queue -v -s
        ;;
    utils)
        echo "Running utility tests on hardware..."
        pytest --target=esp32c3 --port="$ESP_PORT" $PYTEST_FLASH_FLAG pytest_rmaker_common.py::test_rmaker_utils -v -s
        ;;
    cmd_resp)
        echo "Running command response tests on hardware..."
        pytest --target=esp32c3 --port="$ESP_PORT" $PYTEST_FLASH_FLAG pytest_rmaker_common.py::test_rmaker_cmd_resp -v -s
        ;;
    all)
        echo "Running all tests in one session..."
        pytest --target=esp32c3 --port="$ESP_PORT" $PYTEST_FLASH_FLAG pytest_rmaker_common.py::test_all_rmaker_common -v -s
        ;;
    *)
        echo "Running all individual test functions..."
        pytest --target=esp32c3 --port="$ESP_PORT" $PYTEST_FLASH_FLAG pytest_rmaker_common.py -v -s
        ;;
esac

echo ""
echo "Hardware tests completed successfully!"
echo ""
echo "Usage: $0 [--no-flash] [work_queue|utils|cmd_resp|all]"
echo "  --no-flash    Skip flashing device (use existing firmware)"
echo "  work_queue    Run work queue tests only"
echo "  utils         Run utility tests only"
echo "  cmd_resp      Run command response tests only"
echo "  all           Run all tests in one session"
echo "  (no args)     Run all individual test functions"
