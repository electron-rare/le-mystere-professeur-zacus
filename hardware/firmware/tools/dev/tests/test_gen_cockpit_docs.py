import pytest
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from gen_cockpit_docs import format_list

def test_format_list():
    items = ['item1', 'item2', 'item3']
    result = format_list(items)
    assert isinstance(result, str)
    for item in items:
        assert item in result
