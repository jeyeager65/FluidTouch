# RainMaker Common Unit Tests

This directory contains unit tests for the RainMaker Common component.

## Prerequisites

- ESP-IDF (v5.1 or later)
- Any Espressif development board

## Building and Running the Tests

```bash
idf.py build
idf.py -p [PORT] flash monitor
```

## Test Components

The tests cover the following functionalities:

1. **Utility Functions**
   - Time synchronization
   - Timezone management
   - System reboot and reset operations

2. **Work Queue**
   - Task initialization
   - Task scheduling
   - Delayed task execution
   - Task prioritization

## Running Tests with pytest on Hardware

The pytest can run actual Unity tests on ESP32 hardware using pytest-embedded:

### Prerequisites for Hardware Testing:
```bash
# Install pytest-embedded dependencies
pip install -r requirements.txt

# OR install manually
pip install pytest-embedded[serial]
```

### Running Hardware Tests:

```bash
# Connect ESP32 device and run all tests
pytest --target=esp32 --port=/dev/cu.usbserial-* pytest_rmaker_common.py -v -s

# Run specific test groups
pytest --target=esp32 --port=/dev/cu.usbserial-* pytest_rmaker_common.py::test_rmaker_utils -v -s
pytest --target=esp32 --port=/dev/cu.usbserial-* pytest_rmaker_common.py::test_rmaker_cmd_resp -v -s
pytest --target=esp32 --port=/dev/cu.usbserial-* pytest_rmaker_common.py::test_work_queue -v -s

# Or use the helper script (auto-detects port)
./run_tests.sh utils
./run_tests.sh cmd_resp
./run_tests.sh work_queue
./run_tests.sh all

# Skip flashing (use existing firmware)
./run_tests.sh --no-flash work_queue
./run_tests.sh --no-flash utils

# Set custom port
PORT=/dev/ttyUSB0 ./run_tests.sh utils
PORT=/dev/ttyUSB0 ./run_tests.sh --no-flash work_queue
```

## Test Groups

The test app contains the following Unity test groups that run on hardware:

1. **rmaker_utils** (9 test cases) - Utility functions including time sync, timezone, reboot operations
2. **work_queue** (5 test cases) - Work queue initialization, task scheduling, delayed execution
3. **rmaker_cmd_resp** (1 test case) - Command registration and handling

### Expected Test Output:
```
ESP RainMaker Time Sync Init - PASSED
ESP RainMaker Time Check - PASSED
ESP RainMaker Timezone Operations - PASSED
...
Running group: work_queue
Group work_queue completed
```

## Known Issues

1. Some tests require network connectivity for time synchronization.
2. The linter errors related to header files are expected during development as the test project needs to be built with ESP-IDF to resolve includes.
3. The reboot and reset tests use large delays to avoid actually rebooting the device during tests.

## Resolving Include Path Issues

If you're facing linter errors related to missing include paths, you can:

1. Build the project once using `idf.py build`
2. Point your editor to use the compile_commands.json generated in the build directory
3. For VS Code, add this to .vscode/c_cpp_properties.json:

```json
{
    "compileCommands": "${workspaceFolder}/components/rmaker_common/test_apps/build/compile_commands.json"
}
```
