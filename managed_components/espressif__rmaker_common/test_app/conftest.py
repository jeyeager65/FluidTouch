# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
pytest configuration for ESP RainMaker Common test app
"""

import pytest
import os


@pytest.fixture(scope="session")
def test_app_dir():
    """Return the test app directory path"""
    return os.path.dirname(os.path.abspath(__file__))
