import sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import os
import shutil
import tempfile
import pytest
import numpy as np

import matplotlib
matplotlib.use("Agg")

@pytest.fixture()
def tmp_outputs_dir():
    d = tempfile.mkdtemp(prefix="viz_outputs_")
    os.makedirs(os.path.join(d, "snapshots"), exist_ok=True)
    yield d
    shutil.rmtree(d, ignore_errors=True)
