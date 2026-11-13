"""Locate binaries from the selected native build configuration."""

import os
from pathlib import Path


def build_dir(project: Path) -> Path:
    mode = os.environ.get("DFREERNET_BUILD_MODE", "RELEASE")
    if mode not in ("DEBUG", "RELEASE"):
        raise ValueError("DFREERNET_BUILD_MODE must be DEBUG or RELEASE")
    return project / "build" / mode
