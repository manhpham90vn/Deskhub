ifeq ($(OS),Windows_NT)
CODESTYLE := powershell -NoProfile -ExecutionPolicy Bypass -File scripts\codestyle.ps1
CHECKFLAG := -Check
ONLYFLAG  := -Only
DEADCODE  := "$(GIT_BASH)" scripts/dead-code.sh
else
CODESTYLE := scripts/codestyle.sh
CHECKFLAG := --check
ONLYFLAG  := --only
DEADCODE  := scripts/dead-code.sh
endif

format:
	@$(CODESTYLE)

format-cpp:
	@$(CODESTYLE) $(ONLYFLAG) cpp

format-kotlin:
	@$(CODESTYLE) $(ONLYFLAG) kotlin

format-swift:
	@$(CODESTYLE) $(ONLYFLAG) swift

lint:
	@$(CODESTYLE) $(CHECKFLAG)
	@$(DEADCODE)

lint-cpp:
	@$(CODESTYLE) $(CHECKFLAG) $(ONLYFLAG) cpp

lint-kotlin:
	@$(CODESTYLE) $(CHECKFLAG) $(ONLYFLAG) kotlin

lint-swift:
	@$(CODESTYLE) $(CHECKFLAG) $(ONLYFLAG) swift

lint-dead:
	@$(DEADCODE)

ifeq ($(UNAME),Darwin)
lint-dead-swift: quiche-macos opus-macos quiche-ios opus-ios
	@scripts/periphery.sh
else
lint-dead-swift:
	@echo "make $@: needs macOS + Xcode (it builds both Apple apps to index them)"; exit 1
endif

lint-tidy:
	@$(DEVCMD) cmake --preset x64-debug -DDESKHUB_LINUX_APP=OFF >$(NULDEV)
	@$(RUNSH) scripts/clang-tidy.sh

.PHONY: format format-cpp format-kotlin format-swift lint lint-cpp lint-kotlin lint-swift lint-dead lint-dead-swift lint-tidy
