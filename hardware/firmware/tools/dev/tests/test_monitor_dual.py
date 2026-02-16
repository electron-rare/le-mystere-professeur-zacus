import pytest
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from monitor_dual import DualMonitor

def test_dualmonitor_init():
    m = DualMonitor('COM1', 'COM2', baud=9600)
    assert m.port1 == 'COM1'
    assert m.port2 == 'COM2'
    assert m.baud == 9600

def test_open_ports_mock(monkeypatch):
    m = DualMonitor('COM1', 'COM2')
    # Mock serial.Serial to avoid hardware dependency
    class DummySerial:
        def __init__(self, *a, **kw): pass
        def close(self): pass
    monkeypatch.setattr('serial.Serial', DummySerial)
    m.open_ports()
    assert hasattr(m, 'ser1') and hasattr(m, 'ser2')
