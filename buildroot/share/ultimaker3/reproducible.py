"""Give GCC a stable date and avoid checkout-specific source paths in debug info."""
import subprocess

Import("env")
root = env["PROJECT_DIR"]
epoch = subprocess.check_output(["git", "show", "-s", "--format=%ct", "HEAD"], cwd=root).decode().strip()
env["ENV"]["SOURCE_DATE_EPOCH"] = epoch
env.Append(CCFLAGS=["-fdebug-prefix-map=" + root + "=."])
