BUILD_DIR := build
CMAKE     := cmake
CTEST     := ctest
JOBS      := $(shell nproc 2>/dev/null || echo 4)

.PHONY: all test clean

# Configure (if needed) then build.
all:
	@$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug --log-level=WARNING
	@$(CMAKE) --build $(BUILD_DIR) --parallel $(JOBS)

# Run all registered tests; print output only on failure.
test: all
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

clean:
	@$(CMAKE) --build $(BUILD_DIR) --target clean 2>/dev/null || true
	@rm -f $(BUILD_DIR)/test_*
