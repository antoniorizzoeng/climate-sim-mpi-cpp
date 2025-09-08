# Contributing Guidelines

Thank you for considering contributing to **climate-sim-mpi-cpp**!

## Development Workflow
1. Fork the repo and create your branch: `git checkout -b feature/your-feature`
2. Build with CMake, run tests with `ctest`.
3. Ensure code style: run `scripts/format_all.sh`.
4. Submit a Pull Request with clear description.

## Reporting Issues
Use the GitHub Issues page with reproduction steps.

## Code Style
- Follow `.clang-format` rules.

## Testing & Coverage
- All code changes must include unit tests.
- Run the full test suite with:
```bash
  ctest --test-dir build --output-on-failure
  pytest --cov=visualization --cov=src
```
- Coverage must be **≥ 90%** for both the C++ simulation and the Python visualization.
- Pull Requests that reduce coverage below this threshold will not be accepted.

You can generate a coverage report locally:
```bash
gcovr -r . --html --html-details -o coverage.html
```
