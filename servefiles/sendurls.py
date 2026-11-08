#!/usr/bin/env python
# coding: utf-8 -*-

import socket
import struct
import sys
import time

from i18n import tr

try:
    from urlparse import urlparse
except ImportError:
    from urllib.parse import urlparse

if len(sys.argv) < 3:
    print(tr('usage_urls', sys.argv[0]))
    sys.exit(1)

target_ip = sys.argv[1]
file_list_payload = ''

for url in sys.argv[2:]:
    parsed = urlparse(url);
    if not parsed.scheme in ('http', 'https') or parsed.netloc == '':
        print(tr('invalid_url', url))
        sys.exit(1)

    file_list_payload += url + '\n'

file_list_payloadBytes = file_list_payload.encode('ascii')

print(tr('url_list'))
print(file_list_payload)

try:
    print(tr('sending_urls', target_ip))
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((target_ip, 5000))
    sock.sendall(struct.pack('!L', len(file_list_payloadBytes)) + file_list_payloadBytes)
    while len(sock.recv(1)) < 1:
        time.sleep(0.05)
    sock.close()
except Exception as e:
    print(tr('error', str(e)))
    sys.exit(1)
