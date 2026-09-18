.PHONY: install zephyr-setup zephyr-ai-enum zephyr-ai-trace requirements

ZEPHYR_VENV ?= $(HOME)/zephyrproject/.venv/bin/activate
ZEPHYR_BASE ?= $(HOME)/zephyrproject/zephyr
ZEPHYR_SDK_INSTALL_DIR ?= /home/khadem/zephyr-sdk-1.0.1

requirements:
	@echo "Zephyr AI PCIe requirements"
	@echo "- Zephyr SDK: $(ZEPHYR_SDK_INSTALL_DIR)"
	@echo "- Zephyr source tree: $(ZEPHYR_BASE)"
	@echo "- Zephyr venv: $(ZEPHYR_VENV)"
	@echo "- QEMU AArch64: $(ZEPHYR_SDK_INSTALL_DIR)/hosttools/sysroots/x86_64-pokysdk-linux/usr/bin/qemu-system-aarch64"
	@test -f "$(ZEPHYR_VENV)" || { echo "Missing Zephyr venv: $(ZEPHYR_VENV)"; echo "Install or activate the Zephyr environment before proceeding."; exit 1; }
	@test -d "$(ZEPHYR_BASE)" || { echo "Missing Zephyr source tree: $(ZEPHYR_BASE)"; exit 1; }
	@test -d "$(ZEPHYR_SDK_INSTALL_DIR)" || { echo "Missing Zephyr SDK: $(ZEPHYR_SDK_INSTALL_DIR)"; exit 1; }
	@echo "Requirements satisfied"

install: requirements
	@echo "Installing Zephyr AI PCIe environment prerequisites"
	@bash -lc 'source "$(ZEPHYR_VENV)" && west --version >/dev/null 2>&1 || { echo "west is not available in the active Zephyr venv."; exit 1; }'
	@echo "Zephyr environment ready"

zephyr-setup: install
	@echo "ZEPHYR_BASE=$(ZEPHYR_BASE)"
	@echo "ZEPHYR_SDK_INSTALL_DIR=$(ZEPHYR_SDK_INSTALL_DIR)"

zephyr-ai-enum: zephyr-setup
	@bash scripts/run_zephyr_ai_topology.sh

zephyr-ai-trace: zephyr-ai-enum
