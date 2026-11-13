#!/usr/bin/env python3
"""Tests receive forwarding and TX rejection on a namespace-private vcan only."""
from pathlib import Path
from build_mode import build_dir
import json
import os
import socket
import struct
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[1]
assert os.getuid()==0
links=json.loads(subprocess.check_output(['ip','-j','link','show']))
assert {item['ifname'] for item in links}=={'lo'}
subprocess.run(['ip','link','set','lo','up'],check=True)
subprocess.run(['ip','link','add','rnetrx0','type','vcan'],check=True)
subprocess.run(['ip','link','set','rnetrx0','up'],check=True)
directory=Path(tempfile.mkdtemp(prefix='rnet-rx-test-',dir='/tmp'))
with (directory/'bridge.log').open('wb') as log:
    bridge=subprocess.Popen([str(build_dir(root)/'rnet_can_rx_bridge'),'rnetrx0'],stdout=log,stderr=subprocess.STDOUT)
try:
    udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);udp.settimeout(2);udp.connect(('127.0.0.1',39010))
    sender=socket.socket(socket.AF_CAN,socket.SOCK_RAW,socket.CAN_RAW);sender.bind(('rnetrx0',))
    watch=socket.socket(socket.AF_CAN,socket.SOCK_RAW,socket.CAN_RAW);watch.bind(('rnetrx0',));watch.settimeout(.3)
    time.sleep(.15)
    hello=b'RNC1'+bytes([1])+bytes(15);udp.send(hello);ack=udp.recv(64);assert len(ack)==20 and ack[:5]==b'RNC1\x04'
    frame=struct.pack('=IB3x8s',0x80000000|0x1abcdef,3,b'\x01\x02\x03'+bytes(5));sender.send(frame)
    packet=udp.recv(64);assert len(packet)==20 and packet[:6]==b'RNC1\x03\x03'
    assert struct.unpack_from('<I',packet,6)[0]==0x1abcdef and packet[10:13]==b'\x01\x02\x03'
    assert watch.recv(16)==frame
    udp.close();time.sleep(.1)
    udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);udp.settimeout(2);udp.connect(('127.0.0.1',39010))
    udp.send(hello);assert udp.recv(64)[:5]==b'RNC1\x04'
    sender.send(frame);assert udp.recv(64)[10:13]==b'\x01\x02\x03';assert watch.recv(16)==frame
    # The malicious packet asks to transmit a frame. A receive-only bridge
    # must discard it, including from the registered peer. Private vcan only.
    attempt=b'RNC1'+bytes([2,1])+struct.pack('<I',0x123)+b'\xff'+bytes(9)
    assert len(attempt)==20;udp.send(attempt)
    try:unexpected=watch.recv(16)
    except socket.timeout:unexpected=None
    assert unexpected is None,'Receive-only bridge transmitted a CAN frame'
    print('PASS: private vcan receive forwarding, viewer reconnection and explicit CAN-TX datagram rejection')
finally:
    bridge.terminate();bridge.wait(timeout=5)
