"""Print reproducible device metadata; no performance claim."""
import json
import os
import platform
import subprocess

print(json.dumps({
    "machine": platform.node(), "os": platform.platform(), "logical_cpus": os.cpu_count(),
    "cpu": subprocess.check_output(["lscpu"], text=True),
    "compiler": subprocess.check_output(["c++", "--version"], text=True),
}, indent=2))
