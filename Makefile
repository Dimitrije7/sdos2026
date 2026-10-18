# SDOS command facade. Every target runs ci/facade.py, so local builds and CI
# execute the same steps. See docs/commands.md for inputs, outputs, and exit codes.
#
#   make build PRESET=host            build a preset or group (host, board-host, firmware, all)
#   make test                         build and run unit tests on the PC
#   make quality                      every check a pull request runs
#   make help                         list commands and presets

PRESET ?= host-gcc-debug
BOARD ?=
PYTHON ?= python3
FACADE = $(PYTHON) ci/facade.py

COMMANDS = bootstrap configure build test quality package image clean deploy target-smoke

.PHONY: help $(COMMANDS)

help:
	@sed -n '1,9p' Makefile
	@$(PYTHON) -c 'import json; d=json.load(open("CMakePresets.json")); print("Presets:", *[p["name"] for p in d["configurePresets"] if not p.get("hidden")], sep="\n  ")'

$(COMMANDS):
	@$(FACADE) $@ --preset "$(PRESET)" --board "$(BOARD)"
