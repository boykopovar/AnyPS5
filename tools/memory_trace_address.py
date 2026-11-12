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


def parse_range(address, size):
    try:
        start = int(address, 0)
        length = int(size, 0)
    except ValueError as error:
        raise ValueError("address and byte count must be decimal or 0x-prefixed integers") from error
    if start < 0 or start >= 1 << 64:
        raise ValueError("address must be an unsigned 64-bit value")
    if length <= 0 or length >= 1 << 64 or start + length > 1 << 64:
        raise ValueError("byte count must be positive and the queried range must fit in 64 bits")
    return start, length


def overlaps(event, start, size):
    return event.address < start + size and start < event.address + event.size


def select_events(events, start, size):
    return [event for event in events if overlaps(event, start, size)]
