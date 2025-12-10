# Repository Guidelines

## Project Structure & Module Organization
- Core headers live in `include/video_pipeline`; C++ sources are under `src` (`src/core` for framework plumbing, `src/blocks` for concrete sources/sinks, `src/utils` for logging/timing helpers).
- CLI and examples compile from `src/main.cpp` and `examples/*.cpp`; configuration samples sit in `examples/*.yaml|*.conf`.
- Long-form documentation is in `docs/` (architecture, configuration, troubleshooting). Generated build artifacts should stay in `build/`.

## Build, Test, and Development Commands
- Configure and build (release): `mkdir -p build && cd build && cmake .. && make -j$(nproc)`.
- Debug build with extras: `cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON -DENABLE_SANITIZERS=ON ..` from `build/`, then `make`.
- Run the CLI with defaults: `./pipeline_cli`. Load a config: `./pipeline_cli --config ../examples/test_pattern_to_file.yaml --time 5`.
- Example binaries (after build) run from `build/examples/`, e.g., `./simple_test_pattern` or `./performance_test`.
- If tests are added via CTest, run them from `build/` with `ctest --output-on-failure`.

## Coding Style & Naming Conventions
- C++17 codebase; prefer 4-space indentation, no hard tabs.
- File names use `snake_case.cpp/.h`; public classes and structs use `PascalCase`, methods and variables use `camelCase`.
- Keep headers in `include/` interface-only; implementations stay in `src/`. Limit header includes and favor forward declarations to keep compile times low.
- Use `clang-format` (LLVM style with 4 spaces) when available; avoid trailing whitespace and keep lines under ~120 chars.

## Testing Guidelines
- No formal unit suite exists yet; validate changes by running `./pipeline_cli` with representative configs from `examples/` (console, file sink, multi-output).
- When adding tests, place them under a dedicated `tests/` or `src/tests/` folder, name files `*_test.cpp`, and register with CTest so `ctest` runs them.
- For performance-sensitive changes, compare FPS/CPU stats (`--stats`) against prior runs.

## Commit & Pull Request Guidelines
- Use concise, imperative commit messages (e.g., `Add file sink buffer checks`). Group related changes together.
- PRs should describe the change, include reproduction steps or configs, note performance impact, and mention any platform-specific considerations (e.g., V4L2 access).
- Add screenshots or log snippets when touching user-visible behavior or CLI output. Update `docs/` or `examples/` when configs or usage change.

## Configuration & Security Tips
- Sample configurations live in `examples/`; use them as templates rather than editing in place.
- V4L2 or framebuffer access may require `video` group membership; prefer least-privilege instead of `sudo` when running the CLI.
- Keep zero-copy and threading settings explicit in configs to avoid surprises; document defaults in PR descriptions when altering them.
