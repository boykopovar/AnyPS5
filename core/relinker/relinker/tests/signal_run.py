import subprocess


def without_core_dump():
    import resource

    _, hard = resource.getrlimit(resource.RLIMIT_CORE)
    limit = 1 if hard == resource.RLIM_INFINITY or hard >= 1 else 0
    resource.setrlimit(resource.RLIMIT_CORE, (limit, limit))


def run_without_core_dump(command, **options):
    return subprocess.run(command, preexec_fn=without_core_dump, **options)
