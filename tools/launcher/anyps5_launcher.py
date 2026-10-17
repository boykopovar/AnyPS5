#!/usr/bin/env python3
import hashlib
import json
import os
import queue
import shlex
import shutil
import subprocess
import sys
import threading
from pathlib import Path

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
except ImportError:
    tk = None

APP_DIR = Path(__file__).resolve().parent
CONFIG_PATH = APP_DIR / "config.json"
IS_WIN = os.name == "nt"
EXE_EXT = ".exe" if IS_WIN else ".elf"
NO_WINDOW = 0x08000000 if IS_WIN else 0
MODULE_DIRS = ("sce_module", "sce_modules", "prx")
MINGW_DLLS = ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
SELF_MAGICS = (b"\x4f\x15\x3d\x1d", b"\x54\x14\xf5\xee")

DEFAULT_CONFIG = {
    "games_dir": "",
    "anyps5_dir": str(APP_DIR.parent.parent),
    "relinker": "",
    "libs_dir": "",
    "cache_dir": str(APP_DIR / "cache"),
    "extra_args": "",
    "emulators": [],
}


def load_config():
    cfg = dict(DEFAULT_CONFIG)
    try:
        cfg.update(json.loads(CONFIG_PATH.read_text("utf-8")))
    except (OSError, ValueError):
        pass
    return cfg


def save_config(cfg):
    CONFIG_PATH.write_text(json.dumps(cfg, indent=2, ensure_ascii=False), "utf-8")


def find_relinker(cfg):
    p = cfg.get("relinker")
    if p and Path(p).is_file():
        return Path(p)
    build = Path(cfg["anyps5_dir"]) / "build"
    if build.is_dir():
        for name in ("relinker.exe", "relinker"):
            for hit in build.rglob(name):
                if hit.is_file():
                    return hit
    return None


def libs_dir(cfg):
    if cfg.get("libs_dir"):
        return Path(cfg["libs_dir"])
    return Path(cfg["anyps5_dir"]) / "build" / "core" / "libs" / "libs"


class Game:
    def __init__(self, path, name, title_id, icon, elf, kind, module_dir):
        self.path, self.name, self.title_id = path, name, title_id
        self.icon, self.elf, self.kind, self.module_dir = icon, elf, kind, module_dir
        raw = title_id or path.name
        digest = hashlib.sha256(str(Path(path).resolve()).encode()).hexdigest()[:8]
        self.key = "".join(c if c.isalnum() or c in "-_." else "_" for c in raw) + "-" + digest


def read_param(game_dir):
    try:
        data = json.loads((game_dir / "sce_sys" / "param.json").read_text("utf-8", errors="replace"))
    except (OSError, ValueError):
        return "", ""
    if not isinstance(data, dict):
        return "", ""
    tid = str(data.get("titleId") or data.get("title_id") or "")
    lp = data.get("localizedParameters")
    name = ""
    if isinstance(lp, dict):
        for k in (lp.get("defaultLanguage"), "en-US", "en-GB"):
            v = lp.get(k) if k else None
            if isinstance(v, dict) and v.get("titleName"):
                name = v["titleName"]
                break
        if not name:
            for v in lp.values():
                if isinstance(v, dict) and v.get("titleName"):
                    name = v["titleName"]
                    break
    return tid, name


def find_executable(d):
    names = ["eboot.elf", "eboot.bin"]
    names += sorted(p.name for p in d.glob("*.elf") if p.name not in names)
    names += sorted(p.name for p in d.glob("*.bin") if p.name not in names)
    self_hit = None
    for n in names:
        p = d / n
        if not p.is_file():
            continue
        try:
            with open(p, "rb") as f:
                magic = f.read(4)
        except OSError:
            continue
        if magic == b"\x7fELF":
            return p, "elf"
        if magic in SELF_MAGICS and self_hit is None:
            self_hit = p
    if self_hit is None:
        return None, "none"
    try:
        subs = sorted(p for p in d.iterdir() if p.is_dir())
    except OSError:
        subs = []
    for sub in subs:
        for n in ("eboot.elf", "eboot.bin"):
            if read_magic(sub / n) == b"\x7fELF":
                return sub / n, "elf"
    return self_hit, "self"


def scan_games(root, max_depth=4):
    root = Path(root)
    if not root.is_dir():
        return []
    games = []

    def walk(d, depth):
        elf, kind = find_executable(d)
        if kind != "none":
            tid, name = read_param(d)
            icon = d / "sce_sys" / "icon0.png"
            mod = next((d / m for m in MODULE_DIRS if (d / m).is_dir()), None)
            games.append(Game(d, name or d.name, tid, icon if icon.is_file() else None, elf, kind, mod))
            return
        if depth >= max_depth:
            return
        try:
            subs = sorted(p for p in d.iterdir() if p.is_dir())
        except OSError:
            return
        for p in subs:
            walk(p, depth + 1)

    walk(root, 0)
    return games


def read_magic(p):
    try:
        with open(p, "rb") as f:
            return f.read(4)
    except OSError:
        return b""


def extra_module_dirs(game, max_depth=3):
    found = []

    def walk(d, depth):
        try:
            entries = sorted(d.iterdir())
        except OSError:
            return
        if depth > 0 and any(e.name in ("eboot.bin", "eboot.elf") for e in entries):
            return
        if depth > 0 and any(e.is_file() and e.suffix.lower() == ".prx" and read_magic(e) in (b"\x7fELF",) + SELF_MAGICS
                             for e in entries):
            found.append(d.relative_to(game.path).as_posix())
        if depth >= max_depth:
            return
        for e in entries:
            if e.is_dir() and not (depth == 0 and e.name in MODULE_DIRS + ("sce_sys",)):
                walk(e, depth + 1)

    walk(game.path, 0)
    return found


def module_dirs(game):
    return [m for m in MODULE_DIRS if module_source(game, m).is_dir()] + extra_module_dirs(game)


def module_source(game, m):
    near = game.elf.parent / m
    return near if near.is_dir() else game.path / m


def fingerprint(game, cfg, relinker):
    h = hashlib.sha256()

    def add(p):
        st = p.stat()
        h.update(f"{p}|{st.st_size}|{st.st_mtime_ns}\n".encode())

    add(game.elf)
    for m in module_dirs(game):
        for f in sorted(module_source(game, m).iterdir()):
            if f.is_file():
                add(f)
    if relinker:
        add(relinker)
    h.update(f"{cfg.get('extra_args','')}|{sys.platform}".encode())
    return h.hexdigest()


def cache_dir(cfg, game):
    return Path(cfg["cache_dir"]) / game.key


def read_state(out):
    try:
        return json.loads((out / "state.json").read_text("utf-8"))
    except (OSError, ValueError):
        return {}


def game_status(game, cfg):
    rel = find_relinker(cfg)
    if not rel:
        return "No relinker"
    out = cache_dir(cfg, game)
    exe = out / ("app" + EXE_EXT)
    if not exe.exists():
        return "Not converted"
    return "Ready" if read_state(out).get("fingerprint") == fingerprint(game, cfg, rel) else "Outdated"


def stage_source(game, out, log):
    src = out / "src"
    src.mkdir(parents=True, exist_ok=True)
    elf = src / "eboot.elf"
    if game.kind == "self":
        raise RuntimeError(f"{game.elf.name} is a SELF; the launcher takes decrypted ELF dumps only.")
    link_or_copy(game.elf, elf)
    for m in module_dirs(game):
        md = module_source(game, m)
        (src / m).mkdir(parents=True, exist_ok=True)
        for f in sorted(md.iterdir()):
            if not f.is_file() or f.name.endswith(".esbak"):
                continue
            if m not in MODULE_DIRS and f.suffix.lower() != ".prx":
                continue
            if read_magic(f) in SELF_MAGICS:
                raise RuntimeError(f"{m}/{f.name} is a SELF; the launcher takes decrypted ELF dumps only.")
            link_or_copy(f, src / m / f.name)
    return elf


def link_or_copy(s, d):
    try:
        os.link(s, d)
    except OSError:
        shutil.copy2(s, d)


def run_cmd(cmd, cwd, log):
    log("$ " + " ".join(str(c) for c in cmd))
    p = subprocess.Popen([str(c) for c in cmd], cwd=str(cwd), stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, errors="replace",
                         creationflags=NO_WINDOW)
    for line in p.stdout:
        log(line.rstrip())
    return p.wait()


def link_tree(src, dst, skip_top, log):
    copied = 0
    for root, dirs, files in os.walk(src):
        rel = Path(root).relative_to(src)
        if rel == Path("."):
            dirs[:] = [d for d in dirs if d not in skip_top]
        target = dst / rel
        target.mkdir(parents=True, exist_ok=True)
        for f in files:
            s, d = Path(root) / f, target / f
            if d.exists():
                continue
            try:
                os.link(s, d)
            except OSError:
                shutil.copy2(s, d)
                copied += 1
    if copied:
        log(f"Note: {copied} files copied (the cache is on another volume than the game).")


def build_compiler(cfg):
    try:
        cache = (Path(cfg["anyps5_dir"]) / "build" / "CMakeCache.txt").read_text("utf-8", errors="replace")
    except OSError:
        return None
    values = {}
    for line in cache.splitlines():
        key, sep, value = line.partition("=")
        if sep:
            values[key.split(":", 1)[0]] = value.strip()
    compiler = values.get("CMAKE_CXX_COMPILER", "")
    if "/" in compiler or "\\" in compiler:
        return Path(compiler)
    found = shutil.which(compiler) if compiler else None
    if found:
        return Path(found)
    for key in ("CMAKE_CXX_COMPILER_AR", "CMAKE_AR"):
        tool = values.get(key, "")
        if "/" in tool or "\\" in tool:
            return Path(tool).parent / (compiler or "g++")
    return None


def mingw_dirs(cfg):
    dirs = []
    compiler = build_compiler(cfg)
    if compiler is not None:
        dirs.append(compiler.parent)
    found = shutil.which("g++") or shutil.which("gcc")
    if found:
        dirs.append(Path(found).parent)
    return dirs


def sync_libs(out, cfg, log):
    src = libs_dir(cfg)
    prx = sorted(src.glob("*.prx")) if src.is_dir() else []
    if not prx:
        raise RuntimeError(f"No .prx in {src}. Build AnyPS5 first ('Build AnyPS5' button).")
    dst = out / "libs"
    dst.mkdir(exist_ok=True)
    for f in prx:
        d = dst / f.name
        if not d.exists() or d.stat().st_size != f.stat().st_size or d.stat().st_mtime_ns != f.stat().st_mtime_ns:
            shutil.copy2(f, d)
    if IS_WIN:
        dirs = mingw_dirs(cfg) + [src]
        for name in MINGW_DLLS:
            hit = next((d / name for d in dirs if (d / name).is_file()), None)
            if hit is None:
                raise RuntimeError(f"{name} (MinGW runtime) not found next to the compiler of the build "
                                   f"({', '.join(str(d) for d in dirs)}); the game cannot load without it.")
            for target in (dst, out):
                if not (target / name).exists():
                    shutil.copy2(hit, target / name)


def ensure_converted(game, cfg, log, force=False):
    rel = find_relinker(cfg)
    if not rel:
        raise RuntimeError("Relinker not found. Use 'Build AnyPS5' or set it in Settings.")
    out = cache_dir(cfg, game)
    exe = out / ("app" + EXE_EXT)
    fp = fingerprint(game, cfg, rel)
    if not force and exe.exists() and read_state(out).get("fingerprint") == fp:
        log("Already converted: skipping conversion.")
        sync_libs(out, cfg, log)
        return exe
    if game.kind == "self":
        raise RuntimeError(f"{game.elf.name} is a SELF; the launcher takes decrypted ELF dumps only.")
    log(f"Converting {game.name} (first run only)...")
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    elf = stage_source(game, out, log)
    extra = []
    for d in extra_module_dirs(game):
        extra += ["--module-dir", d]
    cmd = [rel] + (["--windows"] if IS_WIN else []) + extra + shlex.split(cfg.get("extra_args", "")) + [elf, exe]
    with open(out / "convert.log", "w", encoding="utf-8") as record:
        def tee(text):
            record.write(text + "\n")
            log(text)
        rc = run_cmd(cmd, out, tee)
    if rc != 0 or not exe.exists():
        raise RuntimeError(f"The relinker failed (exit code {rc}). See the log or {out / 'convert.log'}.")
    app0 = out / "app0"
    app0.mkdir(exist_ok=True)
    link_tree(game.path, app0, MODULE_DIRS, log)
    sync_libs(out, cfg, log)
    (out / "state.json").write_text(json.dumps({"fingerprint": fp, "elf": str(game.elf)}), "utf-8")
    log("Conversion done.")
    return exe


def launch(exe, log):
    out = exe.parent
    if not IS_WIN:
        exe.chmod(exe.stat().st_mode | 0o111)
    log(f"Running {exe.name}...")
    env = dict(os.environ)
    envfile = Path(__file__).with_name("env.txt")
    if envfile.exists():
        for ln in envfile.read_text("utf-8").splitlines():
            ln = ln.strip()
            if ln and not ln.startswith("#") and "=" in ln:
                k, v = ln.split("=", 1)
                env[k.strip()] = v.strip()
    p = subprocess.Popen([str(exe)], cwd=str(out), stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, errors="replace",
                         creationflags=NO_WINDOW, env=env)
    with open(out / "last_run.log", "w", encoding="utf-8") as lf:
        for line in p.stdout:
            lf.write(line)
            log(line.rstrip())
    rc = p.wait()
    log(f"Process exited with code {rc}.")
    return rc


def emulator_command(template, game):
    s = template.format(elf=str(game.elf), dir=str(game.path), id=game.title_id or game.key)
    parts = shlex.split(s, posix=not IS_WIN)
    return [x[1:-1] if len(x) > 1 and x[0] == x[-1] == '"' else x for x in parts]


def build_anyps5(cfg, log):
    root = Path(cfg["anyps5_dir"])
    missing = [t for t in ("git", "cmake", "ninja", "gcc", "g++") if not shutil.which(t)]
    if missing:
        raise RuntimeError("Missing tools in PATH: " + ", ".join(missing)
                           + (" (Windows: MinGW-w64 GCC 15.2.0 winlibs ucrt-posix-seh)" if IS_WIN else ""))
    steps = [
        ["git", "submodule", "update", "--init", "--recursive"],
        ["cmake", "-S", ".", "-B", "build", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
         "-DCMAKE_C_COMPILER=gcc", "-DCMAKE_CXX_COMPILER=g++"],
        ["cmake", "--build", "build", "--parallel"],
        ["cmake", "--build", "build", "--target", "libs", "--parallel"],
    ]
    for s in steps:
        if run_cmd(s, root, log) != 0:
            raise RuntimeError("Failed: " + " ".join(s))
    log("AnyPS5 built.")


class App:
    def __init__(self, root):
        self.root, self.cfg = root, load_config()
        self.q, self.games, self.icons, self.busy = queue.Queue(), {}, {}, False
        root.title("AnyPS5 Launcher")
        root.geometry("920x720")
        root.minsize(720, 520)
        self.build_ui()
        self.poll()
        if self.cfg["games_dir"]:
            self.refresh()

    def build_ui(self):
        r = self.root
        top = ttk.Frame(r, padding=8)
        top.pack(fill="x")
        self.dir_var = tk.StringVar(value=self.cfg["games_dir"])
        ttk.Label(top, text="Games folder:").pack(side="left")
        ttk.Entry(top, textvariable=self.dir_var).pack(side="left", fill="x", expand=True, padx=6)
        ttk.Button(top, text="Browse...", command=self.pick_dir).pack(side="left")
        ttk.Button(top, text="Refresh", command=self.refresh).pack(side="left", padx=4)
        ttk.Button(top, text="Settings", command=self.settings).pack(side="left")

        style = ttk.Style()
        style.configure("Games.Treeview", rowheight=56)
        mid = ttk.Frame(r, padding=(8, 0))
        mid.pack(fill="both", expand=True)
        self.tree = ttk.Treeview(mid, style="Games.Treeview", columns=("id", "state"), selectmode="browse")
        self.tree.heading("#0", text="Game")
        self.tree.heading("id", text="ID")
        self.tree.heading("state", text="Status")
        self.tree.column("#0", width=420)
        self.tree.column("id", width=120)
        self.tree.column("state", width=160)
        sb = ttk.Scrollbar(mid, command=self.tree.yview)
        self.tree.configure(yscrollcommand=sb.set)
        self.tree.pack(side="left", fill="both", expand=True)
        sb.pack(side="left", fill="y")
        self.tree.bind("<Double-1>", lambda e: self.play())

        bar = ttk.Frame(r, padding=8)
        bar.pack(side="bottom", fill="x", before=mid)
        self.buttons = [
            ttk.Button(bar, text="Play", command=self.play),
            ttk.Button(bar, text="Reconvert", command=lambda: self.play(force=True, run=False)),
            ttk.Button(bar, text="Open converted folder", command=self.open_out),
        ]
        for b in self.buttons:
            b.pack(side="left", padx=(0, 6))
        self.emu_btn = ttk.Menubutton(bar, text="Run with emulator")
        self.emu_btn.pack(side="left", padx=(0, 6))
        self.emu_menu = tk.Menu(self.emu_btn, tearoff=0)
        self.emu_btn["menu"] = self.emu_menu
        self.build_btn = ttk.Button(bar, text="Build AnyPS5", command=self.build)
        self.build_btn.pack(side="right")
        self.reload_emulators()

        logframe = ttk.Frame(r, padding=(8, 0, 8, 8))
        logframe.pack(side="bottom", fill="x", before=bar)
        self.logbox = tk.Text(logframe, height=11, state="disabled", wrap="none")
        logsb = ttk.Scrollbar(logframe, command=self.logbox.yview)
        self.logbox.configure(yscrollcommand=logsb.set)
        self.logbox.pack(side="left", fill="both", expand=True)
        logsb.pack(side="left", fill="y")

    def log(self, text):
        self.q.put(("log", text))

    def poll(self):
        try:
            while True:
                m = self.q.get_nowait()
                if m[0] == "log":
                    self.logbox.configure(state="normal")
                    self.logbox.insert("end", m[1] + "\n")
                    self.logbox.see("end")
                    self.logbox.configure(state="disabled")
                elif m[0] == "games":
                    self.fill(m[1])
                elif m[0] == "state":
                    if self.tree.exists(m[1]):
                        self.tree.set(m[1], "state", m[2])
                elif m[0] == "busy":
                    self.busy = m[1]
                elif m[0] == "refresh":
                    self.refresh()
                elif m[0] == "error":
                    messagebox.showerror("AnyPS5 Launcher", m[1])
        except queue.Empty:
            pass
        self.root.after(100, self.poll)

    def worker(self, fn):
        if self.busy:
            return
        self.busy = True

        def run():
            try:
                fn()
            except Exception as e:
                self.log(f"ERROR: {e}")
                self.q.put(("error", str(e)))
            finally:
                self.q.put(("busy", False))
        threading.Thread(target=run, daemon=True).start()

    def selected(self):
        sel = self.tree.selection()
        return self.games.get(sel[0]) if sel else None

    def pick_dir(self):
        d = filedialog.askdirectory(title="Folder with PS5 games")
        if d:
            self.dir_var.set(d)
            self.cfg["games_dir"] = d
            save_config(self.cfg)
            self.refresh()

    def refresh(self):
        self.cfg["games_dir"] = self.dir_var.get().strip()
        save_config(self.cfg)
        cfg = dict(self.cfg)

        def job():
            try:
                games = scan_games(cfg["games_dir"])
                self.q.put(("games", games))
                for g in games:
                    try:
                        self.q.put(("state", g.key, game_status(g, cfg)))
                    except Exception as e:
                        self.q.put(("state", g.key, "Error"))
                        self.log(f"{g.name}: {e}")
            except Exception as e:
                self.log(f"ERROR while scanning: {e}")
        threading.Thread(target=job, daemon=True).start()

    def fill(self, games):
        self.tree.delete(*self.tree.get_children())
        self.games = {g.key: g for g in games}
        self.icons = {}
        for g in games:
            img = ""
            if g.icon:
                try:
                    p = tk.PhotoImage(file=str(g.icon))
                    p = p.subsample(max(1, p.width() // 48))
                    self.icons[g.key] = p
                    img = p
                except tk.TclError:
                    pass
            self.tree.insert("", "end", iid=g.key, text="  " + g.name, image=img,
                             values=(g.title_id, "..."))
        self.log(f"{len(games)} game(s) found.")

    def play(self, force=False, run=True):
        g = self.selected()
        if not g:
            return
        cfg = dict(self.cfg)

        def job():
            exe = ensure_converted(g, cfg, self.log, force=force)
            self.q.put(("state", g.key, "Ready"))
            if run:
                launch(exe, self.log)
        self.worker(job)

    def open_out(self):
        g = self.selected()
        if not g:
            return
        d = cache_dir(self.cfg, g)
        if not d.exists():
            self.log("Not converted yet.")
            return
        if IS_WIN:
            os.startfile(str(d))
        else:
            subprocess.Popen(["xdg-open", str(d)])

    def reload_emulators(self):
        self.emu_menu.delete(0, "end")
        for e in self.cfg.get("emulators", []):
            self.emu_menu.add_command(label=e["name"], command=lambda e=e: self.run_emulator(e))
        if not self.cfg.get("emulators"):
            self.emu_menu.add_command(label="(set up emulators in Settings)", state="disabled")

    def run_emulator(self, emu):
        g = self.selected()
        if not g:
            return
        try:
            cmd = emulator_command(emu["command"], g)
            self.log("$ " + " ".join(cmd))
            subprocess.Popen(cmd)
        except Exception as e:
            self.log(f"ERROR: {e}")

    def build(self):
        cfg = dict(self.cfg)

        def job():
            build_anyps5(cfg, self.log)
            self.q.put(("refresh",))
        self.worker(job)

    def settings(self):
        win = tk.Toplevel(self.root)
        win.title("Settings")
        win.geometry("760x420")
        fields = [("AnyPS5 repository", "anyps5_dir", True), ("Relinker (empty = auto)", "relinker", False),
                  (".prx folder (empty = auto)", "libs_dir", True), ("Cache folder", "cache_dir", True),
                  ("Extra relinker arguments", "extra_args", None)]
        vars_ = {}
        for i, (label, key, is_dir) in enumerate(fields):
            ttk.Label(win, text=label).grid(row=i, column=0, sticky="w", padx=8, pady=4)
            v = tk.StringVar(value=self.cfg.get(key, ""))
            vars_[key] = v
            ttk.Entry(win, textvariable=v, width=60).grid(row=i, column=1, sticky="we", pady=4)
            if is_dir is not None:
                def browse(v=v, is_dir=is_dir):
                    p = filedialog.askdirectory() if is_dir else filedialog.askopenfilename()
                    if p:
                        v.set(p)
                ttk.Button(win, text="...", width=3, command=browse).grid(row=i, column=2, padx=4)
        ttk.Label(win, text="Emulators (one per line: Name = command using {elf} {dir} {id})").grid(
            row=len(fields), column=0, columnspan=3, sticky="w", padx=8, pady=(10, 2))
        emu = tk.Text(win, height=7)
        emu.grid(row=len(fields) + 1, column=0, columnspan=3, sticky="nsew", padx=8)
        emu.insert("1.0", "\n".join(f'{e["name"]} = {e["command"]}' for e in self.cfg.get("emulators", [])))
        win.columnconfigure(1, weight=1)

        def save():
            for k, v in vars_.items():
                self.cfg[k] = v.get().strip()
            emus = []
            for line in emu.get("1.0", "end").splitlines():
                if "=" in line:
                    n, c = line.split("=", 1)
                    if n.strip() and c.strip():
                        emus.append({"name": n.strip(), "command": c.strip()})
            self.cfg["emulators"] = emus
            save_config(self.cfg)
            self.reload_emulators()
            win.destroy()
            self.refresh()
        ttk.Button(win, text="Save", command=save).grid(row=len(fields) + 2, column=1, sticky="e", pady=8)


def main():
    if tk is None:
        sys.exit("Tkinter is not available in this Python.")
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
