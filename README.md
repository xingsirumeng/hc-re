# Homework Collection - Remastered

Due to unfamiliar with golang, I ran into mess of code. In this remaster, I want to reorganize and
write clean code in cpp.

## Features

- Complete flow of assignment submission, management and grading.
- Support computing AIGC rate of submitted assignment.
- Support plagiarism checking for assignments in a same assignment.
- Pretty visual graphs for teachers and students to check the status of
  assignment submission and grading.

## Prerequisite

You should install `cmake`, `perl`.

C++ dependencies are managed by `conan`. You can install `conan` by pip:
```bash
pip install conan
```

Then initialize `conan` by:
```bash
conan profile detect
```

You may use `conan` to install required c++ packages. Here is an example for my computer:
```bash
conan install . -of build -b missing
```

## Quickstart

```bash
cmake --preset conan-default # In some installation, use `conan-release`
cmake --build build
./build/apps/cli/hc
```

## Contributing

If you want to contribute to this project, you may need to read [DEVELOP.md](/DEVELOP.md) first to
get knowledge of tricky part in development.
