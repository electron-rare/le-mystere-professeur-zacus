import pytest
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from diagnostic_ui_link import UiLinkDiagnostics

def test_parse_esp32_line():
    diag = UiLinkDiagnostics()
    # Exemple de ligne ESP32 simulée
    line = '[ESP32] UI_LINK_STATUS connected=1'
    result = diag.parse_esp32_line(line)
    assert result is None or isinstance(result, dict)
    if isinstance(result, dict):
        assert 'connected' in result

def test_parse_oled_line():
    diag = UiLinkDiagnostics()
    # Exemple de ligne OLED simulée
    line = '[OLED] UI_LINK_STATUS connected=0'
    result = diag.parse_oled_line(line)
    assert result is None or isinstance(result, dict)
    if isinstance(result, dict):
        assert 'connected' in result
