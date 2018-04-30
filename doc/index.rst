Welcome to mmlib's documentation!
=================================

mmlib is an `Operating System abstraction layer`__.

.. __: https://en.wikipedia.org/wiki/Operating_system_abstraction_layer

It provides an API for application developer, so that they can write portable
code using relatively advanced OS features without having to care about system
specifics.

It also provides facilities like error reporting or logging that helps
different modules to behaves consistently with each other.

The reference design in mind in its conception is **POSIX**, meaning that if
some posix functionality is not available for given platform, mmlib will
implement that same functionality itself.

**POSIX** is designed by the IEEE since before 1997. Its main goal is
maintaining compatibility between operating systems through standardization.
Therefore, it is a good starting point to design an OS-neutral API.

There are many similarities with **UNIX** systems: historically this system was
chosen because if was manufacturer-neutral.

More infos and link about **POSIX**: `Wikipedia page about posix`__,

.. __: https://en.wikipedia.org/wiki/POSIX

Supported platforms:
 * **POSIX**
 * **Win32**

.. toctree::
   :caption: About implementations choices and design
   :maxdepth: 2

   design.rst


.. toctree::
   :caption: API module list
   :titlesonly:
   :maxdepth: 2

   alloc.rst
   argparse.rst
   dlfcn.rst
   env.rst
   error.rst
   filesystem.rst
   ipc.rst
   log.rst
   process.rst
   profiling.rst
   socket.rst
   thread.rst
   time.rst
   type.rst

.. toctree::
   :caption: Examples
   :maxdepth: 2

   examples.rst

Useful links
============

 * **Sources**: https://intranet.mindmaze.ch/bitbucket/projects/MMLAB/repos/mmlib


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

