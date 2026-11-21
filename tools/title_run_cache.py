import sys


CACHE_PREFIXES = ("MESA_SHADER_CACHE_", "MESA_DISK_CACHE_", "__GL_SHADER_DISK_CACHE")
CACHE_NAMES = ("ANYPS5_SHADER_CACHE_DIR", "ANYPS5_NO_SHADER_CACHE")
DRIVER_PATHS = ("MESA_SHADER_CACHE_DIR", "__GL_SHADER_DISK_CACHE_PATH")
POLICY_KEYS = {"version", "platform", "status", "environment", "overrides", "driver_directory"}
LABELS = {"runner-defaults": "runner-owned defaults configured", "overridden": "cache overrides configured",
          "disable-requested": "cache disable requested", "unknown": "unknown cache configuration"}


def cache_control(name):
    return isinstance(name, str) and (name.upper() in CACHE_NAMES or name.upper().startswith(CACHE_PREFIXES))


def cache_policy_status(environment, overrides, directory, platform):
    if platform != "linux":
        return "unknown"
    disabled = environment.get("ANYPS5_NO_SHADER_CACHE", "")
    if (disabled not in ("", "0") or environment.get("MESA_SHADER_CACHE_DISABLE", "").lower() in ("1", "true")
            or environment.get("__GL_SHADER_DISK_CACHE") == "0"):
        return "disable-requested"
    if overrides or environment != {name: directory for name in DRIVER_PATHS}:
        return "overridden"
    return "runner-defaults"


def configured_cache_policy(environment, extra_env, work):
    settings = {name: value for name, value in environment.items() if cache_control(name)}
    overrides = sorted(name for name in extra_env if cache_control(name))
    directory = str(work / "driver-cache")
    return {"version": 1, "platform": sys.platform,
            "status": cache_policy_status(settings, overrides, directory, sys.platform),
            "environment": settings, "overrides": overrides, "driver_directory": directory}


def recorded_cache_policy(policy):
    if not isinstance(policy, dict) or set(policy) != POLICY_KEYS or type(policy["version"]) is not int or policy["version"] != 1:
        return None
    environment, overrides = policy["environment"], policy["overrides"]
    if (not isinstance(policy["platform"], str) or not policy["platform"]
            or not isinstance(policy["driver_directory"], str) or not policy["driver_directory"].strip()
            or not isinstance(environment, dict)
            or any(name not in environment for name in DRIVER_PATHS)
            or any(not cache_control(name) or "=" in name or "\x00" in name
                   or not isinstance(value, str) or "\x00" in value for name, value in environment.items())
            or not isinstance(overrides, list) or any(not isinstance(name, str) or name not in environment for name in overrides)
            or overrides != sorted(set(overrides))
            or policy["status"] != cache_policy_status(environment, overrides, policy["driver_directory"], policy["platform"])):
        return None
    settings = dict(environment)
    for name in DRIVER_PATHS:
        if name not in overrides and settings.get(name) == policy["driver_directory"]:
            settings[name] = {"scope": "runner-default", "directory": "driver-cache"}
    return {"version": 1, "platform": policy["platform"], "status": policy["status"], "environment": settings, "overrides": overrides}


def common_cache_policy(policies):
    values = [recorded_cache_policy(policy) for policy in policies]
    if not values or any(value is None or value["status"] == "unknown" for value in values):
        return LABELS["unknown"]
    if any(value != values[0] for value in values[1:]):
        return "mixed cache configuration"
    return LABELS[values[0]["status"]]


def cache_policy_label(policy):
    value = recorded_cache_policy(policy)
    if value is None:
        return "unknown cache configuration (not recorded)"
    settings = ", ".join(f"{name}={raw!r}" for name, raw in value["environment"].items() if isinstance(raw, str))
    return LABELS[value["status"]] + (f": {settings}" if settings else "")
