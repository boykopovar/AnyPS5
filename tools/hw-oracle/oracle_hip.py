import argparse
import ctypes
import os
import sys
from functools import cache
from pathlib import Path

HIP_LIBRARIES = ("amdhip64_7.dll", "amdhip64_6.dll", "amdhip64.dll")
COMGR_LIBRARIES = ("amd_comgr_3.dll", "amd_comgr_2.dll", "amd_comgr.dll")
DATA_SOURCE = 0x1
DATA_LOG = 0x5
DATA_RELOCATABLE = 0x7
DATA_EXECUTABLE = 0x8
ASSEMBLE_ACTIONS = (0x8, 0xA)
LINK_ACTIONS = (0x7, 0x9)
HIP_MEMCPY_HOST_TO_DEVICE = 1
HIP_MEMCPY_DEVICE_TO_HOST = 2
DEVICE_PROPERTIES_SIZE = 8192
DEVICE_NAME_SIZE = 256


class OracleError(Exception):
    pass


class Handle(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint64)]


def check(status, what):
    if status != 0:
        raise OracleError(f"{what} failed with status {status}")


def load(variable, names):
    candidates = (os.environ[variable],) if os.environ.get(variable) else names
    errors = []
    for name in candidates:
        try:
            return ctypes.WinDLL(name)
        except OSError as error:
            errors.append(f"{name}: {error}")
    raise OracleError(f"no library found ({variable} overrides it): " + "; ".join(errors))


@cache
def hip():
    if os.environ.get("ROCR_VISIBLE_DEVICES") is not None and "HIP_VISIBLE_DEVICES" not in os.environ:
        os.environ["HIP_VISIBLE_DEVICES"] = os.environ["ROCR_VISIBLE_DEVICES"]
    library = load("HW_ORACLE_HIP", HIP_LIBRARIES)
    library.hipModuleLoadData.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_char_p]
    library.hipModuleGetFunction.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_void_p, ctypes.c_char_p]
    library.hipMemcpy.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
    library.hipMemset.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_size_t]
    library.hipModuleLaunchKernel.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint,
                                              ctypes.c_uint, ctypes.c_uint, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
    library.hipModuleUnload.argtypes = [ctypes.c_void_p]
    library.hipFree.argtypes = [ctypes.c_void_p]
    library.hipHostFree.argtypes = [ctypes.c_void_p]
    check(library.hipInit(0), "hipInit")
    count = ctypes.c_int()
    check(library.hipGetDeviceCount(ctypes.byref(count)), "hipGetDeviceCount")
    if count.value == 0:
        raise OracleError("no HIP device")
    return library


@cache
def comgr():
    library = load("HW_ORACLE_COMGR", COMGR_LIBRARIES)
    library.amd_comgr_create_data.argtypes = [ctypes.c_int, ctypes.POINTER(Handle)]
    library.amd_comgr_create_data_set.argtypes = [ctypes.POINTER(Handle)]
    library.amd_comgr_create_action_info.argtypes = [ctypes.POINTER(Handle)]
    library.amd_comgr_set_data.argtypes = [Handle, ctypes.c_size_t, ctypes.c_char_p]
    library.amd_comgr_set_data_name.argtypes = [Handle, ctypes.c_char_p]
    library.amd_comgr_data_set_add.argtypes = [Handle, Handle]
    library.amd_comgr_action_info_set_isa_name.argtypes = [Handle, ctypes.c_char_p]
    library.amd_comgr_action_info_set_option_list.argtypes = [Handle, ctypes.POINTER(ctypes.c_char_p), ctypes.c_size_t]
    library.amd_comgr_action_info_set_logging.argtypes = [Handle, ctypes.c_bool]
    library.amd_comgr_do_action.argtypes = [ctypes.c_int, Handle, Handle, Handle]
    library.amd_comgr_action_data_count.argtypes = [Handle, ctypes.c_int, ctypes.POINTER(ctypes.c_size_t)]
    library.amd_comgr_action_data_get_data.argtypes = [Handle, ctypes.c_int, ctypes.c_size_t, ctypes.POINTER(Handle)]
    library.amd_comgr_get_data.argtypes = [Handle, ctypes.POINTER(ctypes.c_size_t), ctypes.c_char_p]
    library.amd_comgr_release_data.argtypes = [Handle]
    library.amd_comgr_destroy_data_set.argtypes = [Handle]
    library.amd_comgr_destroy_action_info.argtypes = [Handle]
    return library


@cache
def target():
    properties = ctypes.create_string_buffer(DEVICE_PROPERTIES_SIZE)
    library = hip()
    getter = getattr(library, "hipGetDevicePropertiesR0600", None) or library.hipGetDeviceProperties
    check(getter(properties, 0), "hipGetDeviceProperties")
    start = properties.raw.find(b"gfx", DEVICE_NAME_SIZE)
    if start < 0:
        raise OracleError("the HIP device properties name no gfx target")
    return properties.raw[start:properties.raw.index(b"\0", start)].decode("ascii").split(":")[0]


def data_bytes(data_set, kind):
    library = comgr()
    count = ctypes.c_size_t()
    check(library.amd_comgr_action_data_count(data_set, kind, ctypes.byref(count)), "amd_comgr_action_data_count")
    chunks = []
    for index in range(count.value):
        data = Handle()
        check(library.amd_comgr_action_data_get_data(data_set, kind, index, ctypes.byref(data)), "amd_comgr_action_data_get_data")
        size = ctypes.c_size_t()
        check(library.amd_comgr_get_data(data, ctypes.byref(size), None), "amd_comgr_get_data")
        buffer = ctypes.create_string_buffer(size.value)
        check(library.amd_comgr_get_data(data, ctypes.byref(size), buffer), "amd_comgr_get_data")
        chunks.append(buffer.raw[:size.value])
        library.amd_comgr_release_data(data)
    return chunks


def data_set(kind, name, payload):
    library = comgr()
    data = Handle()
    check(library.amd_comgr_create_data(kind, ctypes.byref(data)), "amd_comgr_create_data")
    check(library.amd_comgr_set_data(data, len(payload), payload), "amd_comgr_set_data")
    check(library.amd_comgr_set_data_name(data, name), "amd_comgr_set_data_name")
    result = Handle()
    check(library.amd_comgr_create_data_set(ctypes.byref(result)), "amd_comgr_create_data_set")
    check(library.amd_comgr_data_set_add(result, data), "amd_comgr_data_set_add")
    library.amd_comgr_release_data(data)
    return result


def comgr_step(actions, isa, options, inputs, kind):
    library = comgr()
    logs = []
    for action in actions:
        info = Handle()
        check(library.amd_comgr_create_action_info(ctypes.byref(info)), "amd_comgr_create_action_info")
        outputs = Handle()
        try:
            check(library.amd_comgr_action_info_set_isa_name(info, isa.encode("ascii")), "amd_comgr_action_info_set_isa_name")
            check(library.amd_comgr_action_info_set_logging(info, True), "amd_comgr_action_info_set_logging")
            if options:
                check(library.amd_comgr_action_info_set_option_list(info, (ctypes.c_char_p * len(options))(*options), len(options)),
                      "amd_comgr_action_info_set_option_list")
            check(library.amd_comgr_create_data_set(ctypes.byref(outputs)), "amd_comgr_create_data_set")
            status = library.amd_comgr_do_action(action, info, inputs, outputs)
            result = data_bytes(outputs, kind)
            log = b"".join(data_bytes(outputs, DATA_LOG)).decode("utf-8", errors="replace").strip()
            library.amd_comgr_destroy_data_set(outputs)
        finally:
            library.amd_comgr_destroy_action_info(info)
        if status == 0 and len(result) == 1:
            return result[0]
        logs.append(f"action {action:#x} status {status}" + (f": {log}" if log else ""))
    raise OracleError("comgr produced no output\n" + "\n".join(logs))


def assemble(source, isa_target, wave64):
    isa = f"amdgcn-amd-amdhsa--{isa_target}"
    inputs = data_set(DATA_SOURCE, b"k.s", source)
    try:
        relocatable = comgr_step(ASSEMBLE_ACTIONS, isa, [b"-mwavefrontsize64"] if wave64 else [], inputs, DATA_RELOCATABLE)
    finally:
        comgr().amd_comgr_destroy_data_set(inputs)
    objects = data_set(DATA_RELOCATABLE, b"k.o", relocatable)
    try:
        return comgr_step(LINK_ACTIONS, isa, [], objects, DATA_EXECUTABLE)
    finally:
        comgr().amd_comgr_destroy_data_set(objects)


def dispatch(code, threads, payload, out_dwords, coarse):
    library = hip()
    module = ctypes.c_void_p()
    check(library.hipModuleLoadData(ctypes.byref(module), code), "hipModuleLoadData")
    try:
        function = ctypes.c_void_p()
        check(library.hipModuleGetFunction(ctypes.byref(function), module, b"probe"), "hipModuleGetFunction")
        out_size = threads * out_dwords * 4
        buffers = []
        try:
            for size in (len(payload), out_size):
                pointer = ctypes.c_void_p()
                if coarse:
                    check(library.hipMalloc(ctypes.byref(pointer), ctypes.c_size_t(size)), "hipMalloc")
                else:
                    check(library.hipHostMalloc(ctypes.byref(pointer), ctypes.c_size_t(size), 0), "hipHostMalloc")
                buffers.append(pointer)
            device_in, device_out = buffers
            check(library.hipMemcpy(device_in, payload, len(payload), HIP_MEMCPY_HOST_TO_DEVICE), "hipMemcpy")
            check(library.hipMemset(device_out, 0xee, out_size), "hipMemset")
            arguments = (ctypes.c_void_p * 2)(ctypes.addressof(device_in), ctypes.addressof(device_out))
            check(library.hipModuleLaunchKernel(function, 1, 1, 1, min(threads, 1024), 1, 1, 0, None, arguments, None), "hipModuleLaunchKernel")
            check(library.hipDeviceSynchronize(), "hipDeviceSynchronize")
            output = ctypes.create_string_buffer(out_size)
            check(library.hipMemcpy(output, device_out, out_size, HIP_MEMCPY_DEVICE_TO_HOST), "hipMemcpy")
            return output.raw
        finally:
            for pointer in buffers:
                (library.hipFree if coarse else library.hipHostFree)(pointer)
    finally:
        library.hipModuleUnload(module)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Assemble and run hw-oracle kernels through the HIP runtime and comgr of AMD's Windows driver.")
    parser.add_argument("--target", action="store_true", help="print the gfx target of the HIP device")
    parser.add_argument("--assemble", nargs=3, metavar=("SOURCE", "OUTPUT", "TARGET"), help="assemble and link SOURCE for TARGET into OUTPUT")
    parser.add_argument("--wave64", action="store_true", help="assemble for wave64")
    parser.add_argument("dispatch", nargs="*", metavar="CODE THREADS IN OUT OUT_DWORDS", help="run CODE as oracle does")
    args = parser.parse_args(argv)
    try:
        if args.target and not args.assemble and not args.dispatch:
            print(target())
            return 0
        if args.assemble and not args.target and not args.dispatch:
            source, output, isa_target = args.assemble
            source, output = Path(source), Path(output)
            source_bytes = source.read_bytes()
            if output.exists() and source.samefile(output):
                parser.error("--assemble output must not overwrite the source")
            output.write_bytes(assemble(source_bytes, isa_target, args.wave64))
            return 0
        if len(args.dispatch) != 5 or args.target or args.assemble or args.wave64:
            parser.error("give --target, --assemble SOURCE OUTPUT TARGET, or CODE THREADS IN OUT OUT_DWORDS")
        code, threads, inputs, output, out_dwords = args.dispatch
        try:
            threads = int(threads)
        except ValueError:
            parser.error("THREADS must be an integer")
        try:
            out_dwords = int(out_dwords)
        except ValueError:
            parser.error("OUT_DWORDS must be an integer")
        if threads not in range(1, 1025) or out_dwords not in range(1, 17):
            parser.error("THREADS must be 1 to 1024 and OUT_DWORDS 1 to 16")
        code, inputs, output = Path(code), Path(inputs), Path(output)
        code_bytes, input_bytes = code.read_bytes(), inputs.read_bytes()
        if output.exists() and any(source.samefile(output) for source in (code, inputs)):
            parser.error("dispatch output must not overwrite an input file")
        if len(input_bytes) < threads * 16:
            parser.error(f"input file must contain at least {threads * 16} bytes")
        result = dispatch(code_bytes, threads, input_bytes, out_dwords, os.environ.get("COARSE") is not None)
        output.write_bytes(result)
        return 0
    except (OSError, OracleError) as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
