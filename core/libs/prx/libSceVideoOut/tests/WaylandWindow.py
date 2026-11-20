import os
import socket
import struct
import subprocess
import sys
import tempfile


def words(*values):
    return struct.pack(f"={len(values)}I", *values)


def wire_string(value):
    encoded = value.encode("ascii") + b"\0"
    return words(len(encoded)) + encoded + bytes(-len(encoded) % 4)


class Compositor:
    def __init__(self, connection, scenario):
        self.connection = connection
        self.scenario = scenario
        self.objects = {1: "wl_display"}
        self.native_shell = None
        self.shell_surfaces = {}
        self.toplevels = {}
        self.decorations = {}
        self.decoration_modes = {}
        self.initial_commits = []

    def send(self, sender, opcode, payload):
        self.connection.sendall(words(sender, (8 + len(payload)) << 16 | opcode) + payload)

    def handle(self, sender, opcode, payload):
        args = struct.unpack(f"={len(payload) // 4}I", payload)
        request = (self.objects[sender], opcode)
        if request == ("wl_display", 0):
            self.send(args[0], 0, words(0))
            self.send(1, 1, words(args[0]))
        elif request == ("wl_display", 1):
            registry = args[0]
            self.objects[registry] = "wl_registry"
            globals_ = [("wl_compositor", 4), ("wl_output", 2), ("xdg_wm_base", 1)]
            if self.scenario == "server-decoration":
                globals_.append(("zxdg_decoration_manager_v1", 1))
            else:
                globals_ += [("wl_shm", 1), ("wl_subcompositor", 1)]
            for name, (interface, version) in enumerate(globals_, 1):
                self.send(registry, 0, words(name) + wire_string(interface) + words(version))
        elif request == ("wl_registry", 0):
            interface = payload[8:8 + args[1] - 1].decode("ascii")
            bound = args[-1]
            self.objects[bound] = interface
            if interface == "wl_output":
                self.send(bound, 0, words(0, 0, 270, 150, 0) + wire_string("AnyPS5") + wire_string("Test") + words(0))
                self.send(bound, 1, words(3, 1280, 720, 60000))
                self.send(bound, 2, b"")
            elif interface == "wl_shm":
                self.send(bound, 0, words(0))
                self.send(bound, 0, words(1))
            elif interface == "xdg_wm_base" and self.native_shell is None:
                self.native_shell = bound
        elif request == ("wl_compositor", 0):
            self.objects[args[0]] = "wl_surface"
        elif request == ("wl_compositor", 1):
            self.objects[args[0]] = "wl_region"
        elif request == ("xdg_wm_base", 2):
            self.objects[args[0]] = "xdg_surface"
            self.shell_surfaces[args[1]] = (args[0], sender != self.native_shell)
        elif request == ("xdg_surface", 1):
            self.objects[args[0]] = "xdg_toplevel"
            self.toplevels[sender] = args[0]
        elif request == ("zxdg_decoration_manager_v1", 1):
            self.objects[args[0]] = "zxdg_toplevel_decoration_v1"
            self.decorations[args[1]] = args[0]
        elif request == ("zxdg_toplevel_decoration_v1", 1):
            self.decoration_modes[sender] = args[0]
        elif request == ("wl_surface", 6) and sender in self.shell_surfaces:
            surface, libdecor = self.shell_surfaces.pop(sender)
            toplevel = self.toplevels[surface]
            decoration = self.decorations.get(toplevel)
            mode = self.decoration_modes.get(decoration, 1) if decoration is not None else None
            self.initial_commits.append((libdecor, mode))
            if self.scenario in ("server-decoration", "libdecor-configure") or (self.scenario == "libdecor-retry" and not libdecor):
                if decoration is not None:
                    self.send(decoration, 0, words(mode))
                self.send(toplevel, 0, words(640, 360, 0))
                self.send(surface, 0, words(1))

    def run(self):
        pending = b""
        while data := self.connection.recv(4096):
            pending += data
            while len(pending) >= 8:
                sender, header = struct.unpack_from("=II", pending)
                size, opcode = header >> 16, header & 0xffff
                if len(pending) < size:
                    break
                self.handle(sender, opcode, pending[8:size])
                pending = pending[size:]


def main(executable, mode):
    server, client = socket.socketpair()
    with server, client, tempfile.TemporaryDirectory() as plugins:
        try:
            server.sendmsg([b"\0"])
            client.recvmsg(1)
        except PermissionError:
            print("Wayland Unix socket messaging is unavailable")
            return 77
        server.settimeout(9)
        env = dict(os.environ, SDL_VIDEODRIVER="wayland", XDG_SESSION_TYPE="wayland",
                   WAYLAND_SOCKET=str(client.fileno()), SDL_VIDEO_WAYLAND_PREFER_LIBDECOR="0",
                   SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR="0" if mode == "configure-timeout" else "1",
                   LIBDECOR_PLUGIN_DIR=plugins)
        env.pop("XDG_ACTIVATION_TOKEN", None)
        with subprocess.Popen([executable, mode], env=env, pass_fds=(client.fileno(),)) as process:
            client.close()
            compositor = Compositor(server, mode)
            try:
                try:
                    compositor.run()
                except (BrokenPipeError, ConnectionResetError):
                    pass
                if process.wait(timeout=1) != 0:
                    raise RuntimeError("The SDL Wayland window check failed")
                expected = {
                    "configure-timeout": [(False, None)],
                    "libdecor-configure": [(True, None)],
                    "libdecor-retry": [(True, None), (False, None)],
                    "server-decoration": [(False, 2)],
                }[mode]
                if compositor.initial_commits != expected:
                    raise RuntimeError(f"Expected initial commits {expected}, got {compositor.initial_commits}")
            finally:
                if process.poll() is None:
                    process.kill()
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))
