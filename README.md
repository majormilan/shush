# Simple Humane Shell (shush)

Welcome to the **Simple Humane Shell** (shush), a statically linked, Bash-compatible Linux shell written in C. The `shush` shell adheres primarily to the [suckless coding standards](https://suckless.org/philosophy/), focusing on simplicity, minimalism, and efficiency. However, when `shush` deviates from these standards, it may be due to a deliberate design choice, a mistake, or an imperfect implementation.

## Features

- **Statically Linked**: `shush` is designed to be statically linked, meaning it includes all necessary libraries at compile time, reducing runtime dependencies.
- **Bash Compatibility**: While being minimalistic, `shush` strives to maintain compatibility with Bash, making it easier to use for those familiar with Bash scripting.
- **Suckless Compliance**: Adheres to the suckless philosophy, keeping the codebase clean, minimal, and efficient.

## State

Currently, `shush` implements only a subset of Bash/POSIX features. Future updates will provide a comprehensive overview of the implemented features and improvements.

## Dependencies

`shush` has minimal dependencies:

- **musl-gcc**: The only external dependency required for building `shush`. It’s used to compile the shell with musl, a lightweight and fast alternative to the standard GNU C library (glibc).

All other necessary components are included within the source code, ensuring `shush` remains lightweight and self-contained.

## License

`shush` is released under the [MIT License](https://github.com/majormilan/shush/blob/master/LICENSE). You are free to use, modify, and distribute the software in accordance with the terms of the license.

