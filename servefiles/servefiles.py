#!/usr/bin/env python
# coding: utf-8 -*-

import os
import socket
import struct
import sys
import threading
import time
import urllib

from i18n import tr

try:
    from SimpleHTTPServer import SimpleHTTPRequestHandler
    from SocketServer import TCPServer
    from urllib import quote
    input = raw_input
except ImportError:
    from http.server import SimpleHTTPRequestHandler
    from socketserver import TCPServer
    from urllib.parse import quote

interactive = False
    
if len(sys.argv) <= 2:
    # Enter interactive mode when arguments are omitted.
    if len(sys.argv) == 2:
        if sys.argv[1].lower() in ('--help', '-help', 'help', 'h', '-h', '--h'):
            print(tr('usage_files', sys.argv[0]))
            sys.exit(1)
    
    interactive = True

elif len(sys.argv) < 3 or len(sys.argv) > 6:
    print(tr('usage_files', sys.argv[0]))
    sys.exit(1)

accepted_extension = ('.cia', '.tik', '.cetk', '.3dsx')
hostPort = 8080

if interactive:
    target_ip = input(tr('prompt_target_ip'))
    target_path = input(tr('prompt_path'))
    
    hostIp = input(tr('prompt_host_ip'))
    if hostIp == '':
        print(tr('detecting_host_ip'))
        hostIp = [(s.connect(('8.8.8.8', 53)), s.getsockname()[0], s.close()) for s in [socket.socket(socket.AF_INET, socket.SOCK_DGRAM)]][0][1]
    else:
        hostPort = input(tr('prompt_host_port'))
        if hostPort == '':
            hostPort = 8080
        else:
            hostPort = int(hostPort)
    

else:
    # Launched with a full Python path.
    if sys.argv[1] == os.path.basename(__file__):
        target_ip = sys.argv[2]
        target_path = sys.argv[3]
        
        if len(sys.argv) >= 5:
            hostIp = sys.argv[4]
            if len(sys.argv) == 6:
                hostPort = int(sys.argv[5])
        else:
            print(tr('detecting_host_ip'))
            hostIp = [(s.connect(('8.8.8.8', 53)), s.getsockname()[0], s.close()) for s in [socket.socket(socket.AF_INET, socket.SOCK_DGRAM)]][0][1]
                
    # Launched directly with the system Python.
    else:
        target_ip = sys.argv[1]
        target_path = sys.argv[2]
        
        if len(sys.argv) >= 4:
            hostIp = sys.argv[3]
            if len(sys.argv) == 5:
                hostPort = int(sys.argv[4])
        else:
            print(tr('detecting_host_ip'))
            hostIp = [(s.connect(('8.8.8.8', 53)), s.getsockname()[0], s.close()) for s in [socket.socket(socket.AF_INET, socket.SOCK_DGRAM)]][0][1]

target_path = target_path.strip()
if not os.path.exists(target_path):
    print(tr('path_missing', target_path))
    sys.exit(1)


print(tr('preparing'))
baseUrl = hostIp + ':' + str(hostPort) + '/'

if os.path.isfile(target_path):
    if target_path.endswith(accepted_extension):
        file_list_payload = baseUrl + quote(os.path.basename(target_path))
        directory = os.path.dirname(target_path)
    else:
        print(tr('unsupported_extension', ', '.join(accepted_extension)))
        sys.exit(1)

else:
    directory = target_path
    file_list_payload = ''
    for file in [file for file in next(os.walk(target_path))[2] if file.endswith(accepted_extension)]:
        file_list_payload += baseUrl + quote(file) + '\n'

if len(file_list_payload) == 0:
    print(tr('no_files'))
    sys.exit(1)

file_list_payloadBytes = file_list_payload.encode('ascii')

if directory and directory != '.':
    os.chdir(directory)

print('\n' + tr('url_list'))
print(file_list_payload + '\n')

class MyServer(TCPServer):
    def server_bind(self):
        import socket
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind(self.server_address)

print(tr('starting_http', hostPort))
server = MyServer(('', hostPort), SimpleHTTPRequestHandler)
thread = threading.Thread(target=server.serve_forever)
thread.start()

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
    server.shutdown()
    sys.exit(1)

print(tr('stopping_http'))
server.shutdown()
