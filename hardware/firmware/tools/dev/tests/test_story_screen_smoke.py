import pytest
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from story_screen_smoke import ScreenTransitionLog

def test_screen_transition_log_record_and_summary():
    log = ScreenTransitionLog()
    log.record(1.0, 'screen1', 'step1', 0)
    log.record(2.0, 'screen2', 'step2', 1)
    summary = log.summary()
    assert isinstance(summary, dict)
    # Les écrans sont dans la clé 'scenes'
    assert 'screen1' in summary['scenes']
    assert 'screen2' in summary['scenes']

def test_print_log(capsys):
    log = ScreenTransitionLog()
    log.record(1.0, 'screen1', 'step1', 0)
    log.record(2.0, 'screen2', 'step2', 1)
    log.print_log()
    captured = capsys.readouterr()
    # print_log affiche les transitions, donc on vérifie la présence de la transition
    assert 'screen1 -> screen2' in captured.out
