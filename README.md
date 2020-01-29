# Overview

mmlib is an [Operating System abstraction layer][1].

It provides an API for application developer, so that they can write portable
code using relatively advanced OS features without having to care about system
specifics.

The reference design in mind in its conception is **POSIX**, meaning that if
some posix functionality is not available for given platform, mmlib will
implement that same functionality itself.

[1]: https://en.wikipedia.org/wiki/Operating_system_abstraction_layer

## Dependencies

### Tests

Running the tests require the `check` framework.

### Documentation

Generating the documentation requires `sphinx` and
[linuxdoc](https://github.com/mmlabs-mindmaze/linuxdoc)

## Install

mmlib supports meson and autotools build systems.

```
# autotools
mkdir build && cd build
../autogen.sh
../configure --prefix=<install-dir>
make
make check # optional
make install

# meson
meson build --prefix=<install-dir>
cd build
ninja
ninja test # optional
ninja install
```

## License

mmlib is free software under the terms of the Apache license 2.0.
See LICENSE file.

## Authors and Contributors

mmlib has been open-sourced by [MindMaze](https://www.mindmaze.com) and is
maintained by Nicolas Bourdaud <nicolas.bourdaud@mindmaze.com>
