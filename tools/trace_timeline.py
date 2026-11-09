import argparse
import html
import sys
import zlib
from pathlib import Path

import trace_digest

DESCRIPTION = """Timeline of an APS5_TRACE_FRAMES / APS5_TRACE_SECONDS capture (aps5-trace-*.json.gz) as one standalone
HTML file: one row per thread, one bar per traced slice, placed on the capture's time axis. Hover a bar for its name and
duration. Slices are clipped to --from-ms and --to-ms. At most --max-slices bars are drawn, longest first, and the page
says how many were left out.

Slices are only what the capture retained; buffer-full losses (see trace_digest.py) leave gaps that look like idle time."""

LABEL_WIDTH = 120
ROW_HEIGHT = 18


