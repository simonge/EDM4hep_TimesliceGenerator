# Copilot Repository Instructions

## Environment
Primary development and CI environment is a pre-built image:  
`ghcr.io/eic/eic_ci:nightly`  
(This image contains ROOT, podio, and the EIC reconstruction stack.)

**Important**: The EIC container requires environment setup. Before building, source any available setup scripts:
- `/opt/detector/setup.sh` (if exists)
- `/opt/software/setup.sh` (if exists) 
- `/usr/local/bin/thisroot.sh` (ROOT setup, if exists)

Set CMAKE_PREFIX_PATH to include common package locations:
```bash
export CMAKE_PREFIX_PATH="/usr/local:$CMAKE_PREFIX_PATH"
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"
```

Avoid rebuilding ROOT or podio from source unless explicitly requested in an issue.

## Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Tests
```bash
ctest --test-dir build --output-on-failure
```

## Agent Guidance
- Always run the build + tests before finalizing a PR.
- If dependencies appear missing in CI, check that the EIC environment setup is being sourced properly.
- Add or update tests whenever modifying logic; place new tests under `tests/` (create if absent).
- Keep commits focused and descriptive.

## Style / Conventions
- Follow existing CMake patterns in root `CMakeLists.txt`.
- Place public headers in `include/`, sources in `src/`.
- Use existing compiler flags (do not introduce global flags unless required).