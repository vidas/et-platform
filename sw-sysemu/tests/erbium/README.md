# Erbium Emulator Tests

Bare-metal tests for the Erbium chip emulator.

## Prerequisites

- RISC-V toolchain (default: `/opt/et/bin`, override with `RISCV=path`)
- Emulator built at `../../build/erbium_emu`

## Usage

```bash
make              # Build all tests
make run          # Run all tests with summary
make run/foo      # Run single test (e.g., make run/dummy_pass)
make list-tests   # List available tests
make clean        # Remove build artifacts
```

Use `VERBOSE=1` for full compiler output.

## Writing Tests

Add a `.c` file to `src/`. Use `TEST_PASS` or `TEST_FAIL` macros from `test.h`:

```c
#include "test.h"

int main() {
    // ... test logic ...
    if (success)
        TEST_PASS;
    else
        TEST_FAIL;
}
```

Tests are auto-discovered from `src/*.c`.
