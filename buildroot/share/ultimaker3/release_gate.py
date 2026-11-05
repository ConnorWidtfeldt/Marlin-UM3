"""Stop the hardware build until the required compatibility inventory is verified."""

import sys
from pathlib import Path

Import("env")
sys.path.insert(0, str(Path(env["PROJECT_DIR"]) / "buildroot/share/ultimaker3"))
from verify import check_release

try:
    check_release()
except ValueError as error:
    print(error, file=sys.stderr)
    env.Exit(1)
