# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from pytest_embedded_idf.dut import IdfDut


def run_unity_group_fast(dut: IdfDut, group: str):
    """Run Unity test group directly without pauses (like manual test)"""
    # Wait for Unity menu prompt
    dut.expect("Press ENTER to see the list of tests")

    # Send ENTER to show menu
    dut.write('')
    dut.expect("Here's the test menu")

    # Wait for the prompt to enter test
    dut.expect("Enter test for running")

    # Send group command directly (like our successful manual test)
    dut.write(f'[{group}]')

    # Wait for completion - look for the summary line
    dut.expect(r"\d+ Tests \d+ Failures \d+ Ignored")


def test_rmaker_utils(dut: IdfDut):
    """Test ESP RainMaker utility functions"""
    run_unity_group_fast(dut, "rmaker_utils")


def test_rmaker_cmd_resp(dut: IdfDut):
    """Test ESP RainMaker command response functionality"""
    run_unity_group_fast(dut, "rmaker_cmd_resp")


def test_work_queue(dut: IdfDut):
    """Test ESP RainMaker work queue functionality"""
    run_unity_group_fast(dut, "work_queue")


def test_all_rmaker_common(dut: IdfDut):
    """Test all ESP RainMaker common component functionality"""
    # Wait for Unity menu prompt
    dut.expect("Press ENTER to see the list of tests")

    # Send ENTER to show menu
    dut.write('')
    dut.expect("Here's the test menu")

    # Wait for the prompt to enter test
    dut.expect("Enter test for running")

    # Run all tests with '*'
    dut.write('*')

    # Wait for completion
    dut.expect(r"\d+ Tests \d+ Failures \d+ Ignored")
