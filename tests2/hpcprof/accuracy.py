#!/usr/bin/env python3

import pickle
import sys

from hpctoolkit.formats import from_path
from hpctoolkit.formats.diff.strict import StrictAccuracy, StrictDiff
from hpctoolkit.test.execution import hpcprof
from hpctoolkit.test.tarball import extracted

threadcnt = int(sys.argv[1])

with open(sys.argv[3], "rb") as f:
    base = pickle.load(f)

with hpcprof(sys.argv[2], f"-j{threadcnt:d}", "--foreign") as db:
    diff = StrictDiff(base, from_path(db.basedir))
    acc = StrictAccuracy(diff)
    if len(diff.hunks) > 0 or acc.inaccuracy:
        diff.render(sys.stdout)
        acc.render(sys.stdout)
        sys.exit(1)