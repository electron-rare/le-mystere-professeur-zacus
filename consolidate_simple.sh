#!/bin/bash
echo "=== Hardware Consolidation Checker ==="
echo ""
echo "Docs to move:"
ls -1 hardware/firmware/*.md 2>/dev/null | sed 's|^|  - |' | grep -v README.md
echo ""
echo "Projects to reorganize:"
ls -d hardware/firmware/esp32_SLIC_phone 2>/dev/null && echo "  - esp32_SLIC_phone"
ls -d hardware/firmware/hardware/RTC_SLIC_PHONE 2>/dev/null && echo "  - RTC_SLIC_PHONE (nested)"
echo ""
echo "Would move to:"
echo "  hardware/docs/ ← all .md files"
echo "  hardware/projects/slic-phone/ ← phone variants"
echo "  hardware/shared/libs/ ← shared libraries"
