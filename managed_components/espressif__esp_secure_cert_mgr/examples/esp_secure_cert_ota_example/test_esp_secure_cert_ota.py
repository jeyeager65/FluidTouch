# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import http.server
import multiprocessing
import os
import ssl
import sys
from typing import Any
import pexpect
import pytest
from pytest_embedded import Dut

try:
    from common_test_methods import get_env_config_variable
    from common_test_methods import get_host_ip4_by_dest_ip
except ModuleNotFoundError:
    idf_path = os.environ['IDF_PATH']
    sys.path.append(idf_path + '/tools/ci')
    sys.path.insert(0, idf_path + '/tools/ci/python_packages')
    from common_test_methods import get_env_config_variable
    from common_test_methods import get_host_ip4_by_dest_ip


def start_https_server(
    ota_image_dir: str,
    server_ip: str,
    server_port: int,
    server_file: str | None = None,
    key_file: str | None = None,
) -> None:
    print(f'Starting HTTPS server on {server_ip}:{server_port}')
    os.chdir(ota_image_dir)

    httpd = http.server.HTTPServer(
        (server_ip, server_port),
        http.server.SimpleHTTPRequestHandler
    )
    print(f'HTTPS server socket: {httpd.socket}')
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_context.load_cert_chain(certfile=server_file, keyfile=key_file)

    httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)
    print(f'HTTPS server started on {server_ip}:{server_port}')
    httpd.serve_forever()


def setting_connection(dut: Dut, env_name: str | None = None) -> Any:
    if (env_name is not None and
            dut.app.sdkconfig.get('EXAMPLE_WIFI_SSID_PWD_FROM_STDIN') is True):
        dut.expect('Please input ssid password:')
        ap_ssid = get_env_config_variable(env_name, 'ap_ssid')
        ap_password = get_env_config_variable(env_name, 'ap_password')
        dut.write(f'{ap_ssid} {ap_password}')
    try:
        ip_address = dut.expect(
            r'IPv4 address: (\d+\.\d+\.\d+\.\d+)[^\d]', timeout=60
        )[1].decode()
        print(f'Connected to AP/Ethernet with IP: {ip_address}')
    except pexpect.exceptions.TIMEOUT:
        raise ValueError('ENV_TEST_FAILURE: Cannot connect to AP/Ethernet')
    return get_host_ip4_by_dest_ip(ip_address)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32c3', 'esp32'], indirect=True)
@pytest.mark.parametrize('config', ['passive_ota', 'unallocated_ota', 'direct_ota'])
def test_esp_secure_cert_ota(dut: Dut):
    # Get the tests directory path
    this_dir = os.path.dirname(os.path.realpath(__file__))
    tests_dir = os.path.join(this_dir, 'tests')
    ota_bin_path = os.path.join(tests_dir, 'esp_secure_cert_after.bin')
    certs_dir = os.path.join(this_dir, 'certs')
    server_cert_path = os.path.join(certs_dir, 'ca_cert.pem')
    server_key_path = os.path.join(certs_dir, 'ca_key.pem')
    server_port = 8001
    ota_filename = os.path.basename(ota_bin_path)
    # Start HTTPS server in background process
    server_process = multiprocessing.Process(
        target=start_https_server,
        args=(
            tests_dir, '0.0.0.0', server_port,
            server_cert_path, server_key_path
        ),
        daemon=True
    )
    server_process.start()

    # Wait for connection and get IP address
    try:
        ip_address = dut.expect(
            r'gw: (\d+\.\d+\.\d+\.\d+)[^\d]', timeout=60
        )[1].decode()
        print(f'Connected to AP/Ethernet with IP: {ip_address}')
    except pexpect.exceptions.TIMEOUT:
        raise ValueError('ENV_TEST_FAILURE: Cannot connect to AP/Ethernet')

    try:
        # Wait for the OTA task to start
        dut.expect('Starting ESP Secure Cert OTA task', timeout=60)

        # Form the OTA URL and write it to stdin
        ota_url = f'https://{ip_address}:{server_port}/{ota_filename}'
        print(f'Writing OTA URL to stdin: {ota_url}')
        dut.write(ota_url + '\n')

        # Continue with the rest of the expectations
        dut.expect(
            'ESP Secure Cert OTA update completed successfully', timeout=60
        )
        dut.expect(
            'You can now restart to use the new certificate data', timeout=60
        )
    finally:
        # Terminate the server process
        server_process.terminate()
        server_process.join(timeout=5)
        if server_process.is_alive():
            server_process.kill()


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32c3', 'esp32'], indirect=True)
@pytest.mark.parametrize('config', ['passive_ota', 'unallocated_ota', 'direct_ota'])
def test_esp_secure_cert_ota_corrupt(dut: Dut):
    # Get the tests directory path
    this_dir = os.path.dirname(os.path.realpath(__file__))
    tests_dir = os.path.join(this_dir, 'tests')
    ota_bin_path = os.path.join(tests_dir, 'esp_secure_cert_after_corrupt.bin')
    certs_dir = os.path.join(this_dir, 'certs')
    server_cert_path = os.path.join(certs_dir, 'ca_cert.pem')
    server_key_path = os.path.join(certs_dir, 'ca_key.pem')
    server_port = 8001
    ota_filename = os.path.basename(ota_bin_path)
    # Start HTTPS server in background process
    server_process = multiprocessing.Process(
        target=start_https_server,
        args=(
            tests_dir, '0.0.0.0', server_port,
            server_cert_path, server_key_path
        ),
        daemon=True
    )
    server_process.start()

    # Wait for connection and get IP address
    try:
        ip_address = dut.expect(
            r'gw: (\d+\.\d+\.\d+\.\d+)[^\d]', timeout=60
        )[1].decode()
        print(f'Connected to AP/Ethernet with IP: {ip_address}')
    except pexpect.exceptions.TIMEOUT:
        raise ValueError('ENV_TEST_FAILURE: Cannot connect to AP/Ethernet')

    try:
        # Wait for the OTA task to start
        dut.expect('Starting ESP Secure Cert OTA task', timeout=60)

        # Form the OTA URL and write it to stdin
        ota_url = f'https://{ip_address}:{server_port}/{ota_filename}'
        print(f'Writing OTA URL to stdin: {ota_url}')
        dut.write(ota_url + '\n')

        # Continue with the rest of the expectations
        dut.expect(
            'Failed to verify integrity of the staging partition after downloading', timeout=60
        )
        dut.expect(
            'ESP Secure Cert OTA failed', timeout=60
        )
    finally:
        # Terminate the server process
        server_process.terminate()
        server_process.join(timeout=5)
        if server_process.is_alive():
            server_process.kill()


if __name__ == '__main__':
    if sys.argv[2:]:  # if two or more arguments provided:
        # Usage: pytest_simple_ota.py <image_dir> <server_port> [cert_dir]
        this_dir = os.path.dirname(os.path.realpath(__file__))
        bin_dir = os.path.join(this_dir, sys.argv[1])
        port = int(sys.argv[2])
        # optional argument
        cert_dir = (bin_dir if not sys.argv[3:]
                    else os.path.join(this_dir, sys.argv[3]))
        print(f'Starting HTTPS server at "https://0.0.0.0:{port}"')
        start_https_server(
            bin_dir,
            '',
            port,
            server_file=os.path.join(cert_dir, 'ca_cert.pem'),
            key_file=os.path.join(cert_dir, 'ca_key.pem'),
        )
