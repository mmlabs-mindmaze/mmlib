# How to contribute

Contributors are essential. Here is some advice to help you help the project.

## Project objectives

We try to keep mmlib as lean and fast as possible and to conform as close as
possible to **POSIX**.

## Submitting pull requests

We use gerrit for code review. If you are unfamiliar with it, please look
at this [gerrit walkthrough](https://gerrit-review.googlesource.com/Documentation/intro-gerrit-walkthrough.html)

Debian provides a package `git-review` that helps submitting git branches to
gerrit for review.

### Coding style

The core mmlib library code follows a coding style enforced by
[uncrustify](https://github.com/uncrustify/uncrustify)
with a configuration file written in tools/uncrustify.cfg

You can also use the build targets `checkstyle` and `fixstyle` to do the check
for you.

### Tests

Please consider adding tests for your new features or that trigger the bug
you are fixing. This will prevent a regression from being unnoticed.

### Code review

Maintainers tend to be picky, and you might feel frustrated that your code
(which is perfectly working in your use case) is not merged faster.

Please don't be offended, and keep in mind that maintainers are concerned
about code maintainability and readability, commit history (we use the
history a lot, for example to find regressions or understand why certain
decisions have been made), performances, integration, API consistency
(so that someone who knows how to use mmlib will know how to use your code), etc.

**Thanks for reading, happy hacking!**
