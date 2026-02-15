import pytest
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from serial_smoke import parse_args

def test_parse_args_default():
    args = parse_args([])
    assert args.role == 'auto'
    assert args.baud == 115200

def test_parse_args_custom():
    args = parse_args(['--role', 'esp32', '--baud', '19200'])
    assert args.role == 'esp32'
    assert args.baud == 19200
