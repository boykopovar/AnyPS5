import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


TRACE_PREFIX = re.compile(r"^\[memory\]\s+")
RANGE = re.compile(r"(?P<address>0x[0-9a-fA-F]+)\+(?P<size>0x[0-9a-fA-F]+)")
OPERATIONS = (
    "remap fixed",
    "map direct",
    "map anon",
    "map file",
    "map shared",
    "protect",
    "unmap released view",
    "unmap",
    "reserve",
)


@dataclass(frozen=True)
class MappingEvent:
    line: int
    operation: str
    address: int
    size: int
    detail: str


def parse_events(lines):
    events = []
    for number, line in enumerate(lines, 1):
        match = TRACE_PREFIX.match(line)
        if match is None:
            continue
        detail = line[match.end():].rstrip("\r\n")
        operation = next((item for item in OPERATIONS if detail.startswith(item + " ")), None)
        if operation is None:
            continue
        address = RANGE.search(detail)
        if address is None:
            raise ValueError(f"line {number}: {operation} event has no address range")
        events.append(MappingEvent(number, operation, int(address["address"], 16),
                                   int(address["size"], 16), detail))
    return events
