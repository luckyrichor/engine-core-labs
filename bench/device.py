"""Read-only environment fingerprint; run directly to emit JSON."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {'.py', '.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp',
                   '.hxx', '.inl', '.cmake', '.sh', '.ps1'}


def command_output(command, cwd=None):
    try:
        return subprocess.check_output(command, cwd=cwd, text=True,
                                       stderr=subprocess.DEVNULL, timeout=15).strip()
    except (OSError, subprocess.SubprocessError):
        return None


def read_text(path):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return None


def linux_memory():
    content = read_text('/proc/meminfo')
    if content is None:
        raise RuntimeError('cannot read /proc/meminfo')
    return {line.split(':')[0]: int(line.split()[1]) for line in content.splitlines()}


def swap_status():
    if sys.platform.startswith('linux'):
        info = linux_memory()
        return {'total_kb': info['SwapTotal'],
                'used_kb': info['SwapTotal'] - info['SwapFree']}
    if sys.platform == 'win32':
        from ctypes import wintypes

        class PageFileInfo(ctypes.Structure):
            _fields_ = [('cb', wintypes.DWORD), ('reserved', wintypes.DWORD),
                        ('total_size', ctypes.c_size_t), ('total_in_use', ctypes.c_size_t),
                        ('peak_usage', ctypes.c_size_t)]

        totals = {'total_kb': 0, 'used_kb': 0}
        system_info = ctypes.create_string_buffer(64)
        ctypes.WinDLL('kernel32', use_last_error=True).GetSystemInfo(system_info)
        page_kb = int.from_bytes(system_info.raw[4:8], 'little') // 1024
        callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, ctypes.c_void_p,
                                          ctypes.POINTER(PageFileInfo), wintypes.LPCWSTR)

        @callback_type
        def callback(context, info, filename):
            totals['total_kb'] += info.contents.total_size * page_kb
            totals['used_kb'] += info.contents.total_in_use * page_kb
            return True

        enumerate_files = ctypes.WinDLL('psapi', use_last_error=True).EnumPageFilesW
        enumerate_files.argtypes = [callback_type, ctypes.c_void_p]
        enumerate_files.restype = wintypes.BOOL
        if not enumerate_files(callback, None):
            raise ctypes.WinError(ctypes.get_last_error())
        return totals
    return {'total_kb': None, 'used_kb': None}


def source_fingerprint(root=ROOT):
    files = {}
    for directory in ('experiments', 'bench', 'tools'):
        for path in sorted((root / directory).rglob('*')):
            if (path.is_file() and '__pycache__' not in path.parts and
                    (path.suffix.lower() in SOURCE_SUFFIXES or path.name == 'CMakeLists.txt')):
                files[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    files = dict(sorted(files.items()))
    # Unambiguous manifest: UTF-8 relative path, NUL, ASCII hex digest, newline.
    manifest = ''.join(f'{path}\0{digest}\n' for path, digest in files.items())
    return {'sha256': hashlib.sha256(manifest.encode('utf-8')).hexdigest(), 'files': files}


def windows_info():
    """Windows API topology, including processor groups (no PowerShell dependency)."""
    from ctypes import wintypes
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    fn = kernel.GetLogicalProcessorInformationEx
    fn.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(wintypes.DWORD)]
    fn.restype = wintypes.BOOL
    size = wintypes.DWORD()
    fn(0, None, ctypes.byref(size))  # RelationProcessorCore
    if not size.value:
        raise ctypes.WinError(ctypes.get_last_error())
    buffer = ctypes.create_string_buffer(size.value)
    if not fn(0, buffer, ctypes.byref(size)):
        raise ctypes.WinError(ctypes.get_last_error())
    cores, offset = [], 0
    pointer_bytes = ctypes.sizeof(ctypes.c_void_p)
    raw = buffer.raw
    while offset < size.value:
        length = int.from_bytes(raw[offset + 4:offset + 8], 'little')
        if length < 32:
            raise RuntimeError('invalid Windows processor topology record')
        efficiency = raw[offset + 9]
        group_count = int.from_bytes(raw[offset + 30:offset + 32], 'little')
        cpus = []
        for index in range(group_count):
            start = offset + 32 + index * (pointer_bytes + 8)
            mask = int.from_bytes(raw[start:start + pointer_bytes], 'little')
            group = int.from_bytes(raw[start + pointer_bytes:start + pointer_bytes + 2], 'little')
            cpus.extend(group * 64 + bit for bit in range(pointer_bytes * 8) if mask & (1 << bit))
        cores.append((efficiency, cpus))
        offset += length

    class MemoryStatus(ctypes.Structure):
        _fields_ = [('length', wintypes.DWORD), ('load', wintypes.DWORD)] + [
            (name, ctypes.c_ulonglong) for name in ('total_phys', 'avail_phys',
            'total_pagefile', 'avail_pagefile', 'total_virtual', 'avail_virtual', 'extended')]
    memory = MemoryStatus()
    memory.length = ctypes.sizeof(memory)
    if not kernel.GlobalMemoryStatusEx(ctypes.byref(memory)):
        raise ctypes.WinError(ctypes.get_last_error())
    classes = {eff for eff, _ in cores}
    highest, lowest = max(classes), min(classes)
    import winreg
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                        r'HARDWARE\DESCRIPTION\System\CentralProcessor\0') as key:
        model = winreg.QueryValueEx(key, 'ProcessorNameString')[0].strip()
    return dict(cpu_model=model, physical_cores=len(cores),
                logical_threads=sum(len(cpus) for _, cpus in cores),
                memory_total_kb=memory.total_phys // 1024,
                performance_cpus=[cpu for eff, cpus in cores for cpu in cpus if eff == highest],
                efficiency_cpus=[cpu for eff, cpus in cores for cpu in cpus
                                 if len(classes) > 1 and eff == lowest],
                processor_cores=[{'efficiency_class': eff, 'cpus': cpus} for eff, cpus in cores],
                power_plan=command_output(['powercfg', '/getactivescheme']))


def collect(*, compiler=None, compile_options=None, dataset_checksums=None,
            external_libraries=None, threads=None, root=ROOT):
    result = dict(machine=platform.node(), system=platform.system(), kernel=platform.release(),
                  platform=platform.platform(), logical_threads=os.cpu_count(),
                  performance_cpus=None, efficiency_cpus=None, power_plan=None,
                  physical_cores=None, cpu_model=platform.processor(), memory_total_kb=None)
    if sys.platform.startswith('linux'):
        cpuinfo = read_text('/proc/cpuinfo') or ''
        blocks = [dict(line.split(':', 1) for line in block.splitlines() if ':' in line)
                  for block in cpuinfo.split('\n\n') if block.strip()]
        blocks = [{key.strip(): value.strip() for key, value in block.items()} for block in blocks]
        pairs = {(block['physical id'], block['core id']) for block in blocks
                 if 'physical id' in block and 'core id' in block}
        result.update(cpu_model=next((b['model name'] for b in blocks if 'model name' in b), None),
                      physical_cores=len(pairs) if pairs else None,
                      memory_total_kb=linux_memory()['MemTotal'],
                      transparent_hugepage={name: read_text('/sys/kernel/mm/transparent_hugepage/' + name)
                                            for name in ('enabled', 'defrag')})
    elif sys.platform == 'win32':
        result.update(windows_info())
    result.setdefault('transparent_hugepage', None)
    compiler = compiler or (['cl'] if sys.platform == 'win32' else [os.environ.get('CXX', 'c++'), '--version'])
    if not isinstance(compiler, list):
        raise ValueError('compiler must be a command argument list')
    status = command_output(['git', 'status', '--porcelain', '--untracked-files=all'], cwd=root)
    # Plan section A: more threads than physical cores is oversubscription.
    cores = result.get('physical_cores') or result['logical_threads']
    result.update(compiler={'command': compiler, 'version': command_output(compiler)},
                  compile_options=compile_options or [], source=source_fingerprint(root),
                  git={'head': command_output(['git', 'rev-parse', 'HEAD'], cwd=root),
                       'dirty': bool(status) if status is not None else None},
                  dataset_checksums=dataset_checksums or {}, external_libraries=external_libraries or [],
                  requested_threads=threads or [],
                  oversubscribed=any(t > cores for t in (threads or [])) if cores else None,
                  swap=swap_status())
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-options', nargs='*', default=[])
    args = parser.parse_args()
    print(json.dumps(collect(compile_options=args.compile_options), indent=2, ensure_ascii=False))


if __name__ == '__main__':
    main()
