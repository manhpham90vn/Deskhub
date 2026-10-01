icons:
	@$(PYTHON) scripts/make-icons.py

quic-smoke:
	@$(RUNSH) scripts/quic-smoke.sh

opus-smoke:
	@$(RUNSH) scripts/opus-smoke.sh

ifeq ($(UNAME),Darwin)
screenshots:
	@scripts/store-screenshots.sh $(ARGS)
else ifeq ($(OS),Windows_NT)
screenshots:
	@echo make $@: needs macOS + Xcode, it drives the iOS Simulator and the macOS app && exit /b 1
else
screenshots:
	@echo "make $@: needs macOS + Xcode (it drives the iOS Simulator and the macOS app)"; exit 1
endif

.PHONY: icons quic-smoke opus-smoke screenshots
