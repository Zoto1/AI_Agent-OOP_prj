# How to compile for linux

```basch
cmake -S . -B build
cmake --build build
```

## How to run

```basch
Please check dir before run (you must in src/)
./build/agent_run (for asking agent)
./build/benchmark_run <config.json-dir> src/benchmark/task.json (benchmark mode)
```
