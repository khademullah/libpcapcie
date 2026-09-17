# Contributing to libpcapcie

Thanks for your interest in contributing to this project.

## Workflow

1. Fork the repository and create a feature branch.
2. Keep changes focused and easy to review.
3. Add or update tests when relevant.
4. Run the relevant build and validation steps.
5. Open a pull request with a clear summary and motivation.

## Coding conventions

- Prefer small, readable C changes.
- Keep public APIs documented.
- Maintain compatibility with the existing backend abstraction model.
- Avoid introducing heavyweight dependencies.

## Building locally

```bash
cmake -S . -B build
cmake --build build
```

## Reporting issues

Please include:
- platform details
- backend used
- exact commands run
- observed output
- expected behavior
