# Copilot Repository Instructions

## Environment
Primary development and CI environment is a pre-built image:  
`ghcr.io/eic/eic_ci@sha256:REPLACE_WITH_REAL_DIGEST`  
(This image contains ROOT, podio, and the EIC reconstruction stack.)

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
- If dependencies appear missing, do NOT attempt ad-hoc `apt-get install` ROOT—assume CI container will supply them and just proceed with code changes unless the task is explicitly about build infrastructure.
- Add or update tests whenever modifying logic; place new tests under `tests/` (create if absent).
- Keep commits focused and descriptive.

## Style / Conventions
- Follow existing CMake patterns in root `CMakeLists.txt`.
- Place public headers in `include/`, sources in `src/`.
- Use existing compiler flags (do not introduce global flags unless required).