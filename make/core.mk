ifeq ($(OS),Windows_NT)
CORE_TESTS := out\build\x64-debug\core\core_tests.exe
PLAT_TESTS := out\build\x64-debug\platform\platform_tests.exe
INTEG_TESTS := out\build\x64-debug\tests\integration\integration_tests.exe
COV_TESTS  := out\build\coverage\core\core_tests.exe
COV_RAW    := out\build\coverage\core_tests.profraw
COV_DATA   := out\build\coverage\core_tests.profdata
COV_OUT    := out\coverage
COV_SRC    := core\src core\include
PERF_BIN   := out\build\x64-release\core\core_perf.exe
PERF_BASE  := out\perf\baseline.txt
PLAT_PERF_BIN  := out\build\x64-release\platform\platform_perf.exe
PLAT_PERF_BASE := out\perf\platform-baseline.txt
else
CORE_TESTS := out/build/x64-debug/core/core_tests
PLAT_TESTS := out/build/x64-debug/platform/platform_tests
INTEG_TESTS := out/build/x64-debug/tests/integration/integration_tests
COV_TESTS  := out/build/coverage/core/core_tests
COV_RAW    := out/build/coverage/core_tests.profraw
COV_DATA   := out/build/coverage/core_tests.profdata
COV_OUT    := out/coverage
COV_SRC    := core/src core/include
PERF_BIN   := out/build/x64-release/core/core_perf
PERF_BASE  := out/perf/baseline.txt
PLAT_PERF_BIN  := out/build/x64-release/platform/platform_perf
PLAT_PERF_BASE := out/perf/platform-baseline.txt
endif

quiche:
	-@$(QUICHE)

opus:
	-@$(OPUS)

debug: quiche opus
	@$(DEVCMD) cmake --preset x64-debug && cmake --build --preset x64-debug

release: quiche opus
	@$(DEVCMD) cmake --preset x64-release && cmake --build --preset x64-release

test:
	@$(DEVCMD) cmake --preset x64-debug >$(NULDEV) && cmake --build --preset x64-debug --target core_tests
	@echo ===== Running core_tests offline =====
	$(CORE_TESTS)

test-platform:
	@$(DEVCMD) cmake --preset x64-debug >$(NULDEV) && cmake --build --preset x64-debug --target platform_tests
	@echo ===== Running platform_tests locally =====
	$(PLAT_TESTS)

test-integration:
	@$(DEVCMD) cmake --preset x64-debug >$(NULDEV) && cmake --build --preset x64-debug --target integration_tests
	@echo ===== Running integration_tests locally =====
	$(INTEG_TESTS)

test-all: test test-platform test-integration

perf-build: quiche opus
	@$(DEVCMD) cmake --preset x64-release >$(NULDEV) && cmake --build --preset x64-release --target core_perf platform_perf

ifeq ($(OS),Windows_NT)
test-perf: perf-build
	@echo ===== Running core_perf against $(PERF_BASE) =====
	$(PERF_BIN)
	@echo ===== Running platform_perf against $(PLAT_PERF_BASE) =====
	$(PLAT_PERF_BIN)

perf-baseline: perf-build
	@$(DEVCMD) cmake -E make_directory out\perf
	@$(DEVCMD) set "DESKHUB_PERF_BASELINE=$(NULDEV)" && set "DESKHUB_PERF_WRITE=$(PERF_BASE)" && $(PERF_BIN)
	@$(DEVCMD) set "DESKHUB_PERF_BASELINE=$(NULDEV)" && set "DESKHUB_PERF_WRITE=$(PLAT_PERF_BASE)" && $(PLAT_PERF_BIN)
else
test-perf: perf-build
	@echo "===== Running core_perf against $(PERF_BASE) ====="
	$(PERF_BIN)
	@echo "===== Running platform_perf against $(PLAT_PERF_BASE) ====="
	$(PLAT_PERF_BIN)

perf-baseline: perf-build
	@cmake -E make_directory out/perf
	DESKHUB_PERF_BASELINE=$(NULDEV) DESKHUB_PERF_WRITE=$(PERF_BASE) $(PERF_BIN)
	DESKHUB_PERF_BASELINE=$(NULDEV) DESKHUB_PERF_WRITE=$(PLAT_PERF_BASE) $(PLAT_PERF_BIN)
endif

FUZZ_TARGETS := fuzz_wire fuzz_annexb fuzz_h264sps fuzz_reassembler fuzz_session fuzz_uitext fuzz_term fuzz_keys fuzz_qr
FUZZ_SECONDS ?= 30
FUZZ_COV_BIN := out/build/fuzz-coverage/core/$(firstword $(FUZZ_TARGETS))
FUZZ_COV_OBJS := $(FUZZ_COV_BIN) $(foreach t,$(wordlist 2,$(words $(FUZZ_TARGETS)),$(FUZZ_TARGETS)),-object out/build/fuzz-coverage/core/$(t))
FUZZ_COV_DATA := out/build/fuzz-coverage/fuzz.profdata

ifeq ($(OS),Windows_NT)
test-asan test-tsan fuzz fuzz-coverage:
	@echo make $@: needs clang or gcc on Linux/macOS, not MSVC && exit /b 1
else
test-asan:
	@cmake --preset asan >$(NULDEV) && cmake --build --preset asan --target core_tests platform_tests integration_tests
	@ctest --test-dir out/build/asan --output-on-failure

test-tsan:
	@cmake --preset tsan >$(NULDEV) && cmake --build --preset tsan --target core_tests platform_tests integration_tests
	@ctest --test-dir out/build/tsan --output-on-failure

fuzz:
	@cmake --preset fuzz >$(NULDEV) && cmake --build --preset fuzz --target $(FUZZ_TARGETS)
	@for t in $(FUZZ_TARGETS); do \
		mkdir -p out/fuzz/corpus/$$t; \
		out/build/fuzz/core/$$t -runs=0 core/fuzz/regressions/$$t || exit 1; \
		out/build/fuzz/core/$$t -max_total_time=$(FUZZ_SECONDS) -max_len=2048 \
			-dict=core/fuzz/dict/$$t.dict \
			out/fuzz/corpus/$$t core/fuzz/seeds/$$t || exit 1; \
	done

fuzz-coverage:
	@cmake --preset fuzz-coverage >$(NULDEV) && cmake --build --preset fuzz-coverage --target $(FUZZ_TARGETS)
	@for t in $(FUZZ_TARGETS); do \
		mkdir -p out/fuzz/corpus/$$t; \
		LLVM_PROFILE_FILE=out/build/fuzz-coverage/$$t.profraw \
			out/build/fuzz-coverage/core/$$t -runs=0 \
			out/fuzz/corpus/$$t core/fuzz/seeds/$$t core/fuzz/regressions/$$t || exit 1; \
	done
	@$(LLVM) llvm-profdata merge -sparse out/build/fuzz-coverage/*.profraw -o $(FUZZ_COV_DATA)
	@$(LLVM) llvm-cov show $(FUZZ_COV_OBJS) -instr-profile=$(FUZZ_COV_DATA) -format=html -output-dir=out/fuzz-coverage $(COV_SRC)
	@$(LLVM) llvm-cov report $(FUZZ_COV_OBJS) -instr-profile=$(FUZZ_COV_DATA) $(COV_SRC)
	@echo "Report: out/fuzz-coverage/index.html"
endif

test-ctest:
	@$(DEVCMD) cmake --preset x64-debug >$(NULDEV) && cmake --build --preset x64-debug --target core_tests
	@$(DEVCMD) cmake --build --preset x64-debug --target platform_tests
	@$(DEVCMD) cmake --build --preset x64-debug --target integration_tests
	@$(DEVCMD) ctest --test-dir out/build/x64-debug --output-on-failure

ifeq ($(OS),Windows_NT)
coverage:
	@$(LLVMPATH) $(DEVCMD) cmake --preset coverage >$(NULDEV) && cmake --build --preset coverage --target core_tests
	@$(DEVCMD) set "LLVM_PROFILE_FILE=$(COV_RAW)" && $(COV_TESTS)
	@$(LLVMPATH) $(DEVCMD) llvm-profdata merge -sparse $(COV_RAW) -o $(COV_DATA)
	@$(LLVMPATH) $(DEVCMD) llvm-cov show $(COV_TESTS) -instr-profile=$(COV_DATA) -format=html -output-dir=$(COV_OUT) $(COV_SRC)
	@$(LLVMPATH) $(DEVCMD) llvm-cov report $(COV_TESTS) -instr-profile=$(COV_DATA) $(COV_SRC)
	@echo Report: $(COV_OUT)\index.html
else
coverage:
	@cmake --preset coverage >$(NULDEV) && cmake --build --preset coverage --target core_tests
	LLVM_PROFILE_FILE=$(COV_RAW) $(COV_TESTS)
	@$(LLVM) llvm-profdata merge -sparse $(COV_RAW) -o $(COV_DATA)
	@$(LLVM) llvm-cov show $(COV_TESTS) -instr-profile=$(COV_DATA) -format=html -output-dir=$(COV_OUT) $(COV_SRC)
	@$(LLVM) llvm-cov report $(COV_TESTS) -instr-profile=$(COV_DATA) $(COV_SRC)
	@echo "Report: $(COV_OUT)/index.html"
endif

.PHONY: quiche opus debug release test test-platform test-integration test-all perf-build test-perf perf-baseline test-asan test-tsan test-ctest coverage fuzz fuzz-coverage
