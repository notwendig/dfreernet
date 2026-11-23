#!/usr/bin/env python3
"""Start the original IRConfigurator with the staged Wine/CAN runtime."""
import sys
from start_oem import main

if __name__ == '__main__':
    sys.exit(main('ir'))
