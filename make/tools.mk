icons:
	@$(PYTHON) scripts/make-icons.py

quic-smoke:
	@$(RUNSH) scripts/quic-smoke.sh

opus-smoke:
	@$(RUNSH) scripts/opus-smoke.sh

screenshots:
	@$(RUNSH) scripts/store-screenshots.sh $(ARGS)

encoder-bake-off:
	@$(RUNSH) scripts/encoder-bake-off.sh $(ARGS)

.PHONY: icons quic-smoke opus-smoke screenshots encoder-bake-off
