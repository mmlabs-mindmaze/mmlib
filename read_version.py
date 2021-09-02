#!/usr/bin/env python3

from pathlib import Path
from sys import stdout

stdout.write(open(Path(__file__).parent / 'VERSION').read())
