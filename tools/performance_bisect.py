import argparse
import json
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parent
PLACEHOLDERS = ("worktree", "build_dir", "result_dir", "commit")
TITLE_ID = re.compile(r"[A-Z]{4}[0-9]{5}")
RECIPE_FIELDS = {"title_id", "build_commands", "build_dir", "games", "seconds", "repetitions",
                 "regression_pct", "max_spread_pct", "titles", "content"}


def load_recipe(path):
    recipe = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(recipe, dict):
        raise ValueError("recipe must be a JSON object")
    unknown = set(recipe) - RECIPE_FIELDS
    if unknown:
        raise ValueError(f"recipe has unknown fields: {', '.join(sorted(unknown))}")
    title_id = recipe.get("title_id")
    if not isinstance(title_id, str) or not TITLE_ID.fullmatch(title_id):
        raise ValueError("recipe.title_id must be a title ID such as PPSA00000")
    commands = recipe.get("build_commands")
    if not isinstance(commands, list) or not commands:
        raise ValueError("recipe.build_commands must contain at least one command")
    for number, command in enumerate(commands, 1):
        if not isinstance(command, list) or not command or any(not isinstance(item, str) or not item for item in command):
            raise ValueError(f"recipe.build_commands[{number - 1}] must be a non-empty argument list")
    build_dir = recipe.get("build_dir", "build")
    if not isinstance(build_dir, str) or not build_dir or Path(build_dir).is_absolute() or ".." in Path(build_dir).parts:
        raise ValueError("recipe.build_dir must be a relative path inside the worktree")
    games = recipe.get("games")
    if not isinstance(games, list) or not games or any(not isinstance(item, str) or not Path(item).is_dir() for item in games):
        raise ValueError("recipe.games must list existing extracted-game directories")
    seconds = recipe.get("seconds", 60)
    if type(seconds) is not int or seconds < 20:
        raise ValueError("recipe.seconds must be an integer of at least 20")
    repetitions = recipe.get("repetitions", 3)
    if type(repetitions) is not int or repetitions < 3:
        raise ValueError("recipe.repetitions must be an integer of at least 3")
    regression_pct = recipe.get("regression_pct", 5)
    if type(regression_pct) not in (int, float) or not math.isfinite(regression_pct) or not 0 < regression_pct < 100:
        raise ValueError("recipe.regression_pct must be between 0 and 100")
    max_spread_pct = recipe.get("max_spread_pct", 3)
    if type(max_spread_pct) not in (int, float) or not math.isfinite(max_spread_pct) or not 0 < max_spread_pct < 100:
        raise ValueError("recipe.max_spread_pct must be between 0 and 100")
    titles = recipe.get("titles", str(TOOLS_DIR / "titles.json"))
    if not isinstance(titles, str):
        raise ValueError("recipe.titles must be a path string")
    if not Path(titles).is_file():
        raise ValueError("recipe.titles must name an existing title catalog")
    content = recipe.get("content")
    if content is not None:
        if not isinstance(content, str) or not Path(content).is_dir():
            raise ValueError("recipe.content must name an existing content directory")
    recipe.update(build_dir=build_dir, seconds=seconds, repetitions=repetitions,
                  regression_pct=float(regression_pct), max_spread_pct=float(max_spread_pct),
                  titles=str(Path(titles).resolve()), content=str(Path(content).resolve()) if content is not None else None)
    return recipe
