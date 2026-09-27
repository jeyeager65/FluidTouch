#!/bin/bash

# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

# Setup script for ESP RainMaker Common hardware testing

set -e

echo "Setting up ESP RainMaker Common Hardware Tests..."
echo ""

# Check if we're in ESP-IDF environment
if [ -z "$IDF_PATH" ]; then
    echo "ERROR: ESP-IDF environment not detected!"
    echo "Please run: source \$IDF_PATH/export.sh"
    exit 1
fi

echo "ESP-IDF environment detected: $IDF_PATH"

# Install pytest-embedded dependencies
echo "Installing pytest-embedded dependencies..."
pip install -r requirements.txt

echo ""
echo "Setup complete!"
echo ""
echo "Now you can run hardware tests:"
echo "  ./run_tests.sh utils      # Test utility functions"
echo "  ./run_tests.sh work_queue # Test work queue"
echo "  ./run_tests.sh cmd_resp   # Test command response"
echo "  ./run_tests.sh            # Run all tests"
echo ""
echo "Make sure to connect an ESP32 device before running tests!"
