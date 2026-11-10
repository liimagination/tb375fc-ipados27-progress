"""Audit the expanded carrier and export an unflashed, reversible boot test."""
from pathlib import Path
import ctypes
import hashlib
import json
import lzma
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import uuid

p = Path(__file__).resolve().parents[1]
w = Path('/root/work/tb375fc-image4-test27')
mu = w/'Mu-Silicium'
assert 'BUILD OK' in (w/'build-lzma.log').read_text()
fv = mu/'Build/tb375fcPkg/DEBUG_CLANGPDB/FV'
fd = (fv/'SILICIUM_UEFI.fd').read_bytes()
assert len(fd) == 0x02000000
payload = (mu/'Resources/FirstXnu/payload.img').read_bytes()
guid = '4238B7F0-0271-4B91-A5A1-827375FC0027'
raw = (fv/f'Ffs/{guid}FVMAIN/{guid}SEC1.raw').read_bytes()
header = 8 if raw[:3] == b'\xff\xff\xff' else 4
assert raw[3] == 0x19 and lzma.decompress(raw[header:], format=lzma.FORMAT_ALONE) == payload
tag = uuid.UUID('ee4e5898-3914-4259-9d6e-dc7bd79403cf').bytes_le
locations = []
for match in re.finditer(re.escape(tag), fd):
    for header_bytes in (4, 8):
        at = match.start()-header_bytes
        if at < 0 or fd[at+3] != 2:
            continue
        extended = fd[at:at+3] == b'\xff\xff\xff'
        if extended != (header_bytes == 8):
            continue
        length = struct.unpack_from('<I', fd, at+4)[0] if extended else int.from_bytes(fd[at:at+3], 'little')
        if header_bytes+20 <= length <= len(fd)-at:
            locations.append((at, header_bytes, length))
assert len(locations) == 1, locations
at, header_bytes, length = locations[0]
offset = struct.unpack_from('<H', fd, at+header_bytes+16)[0]
assert header_bytes+20 <= offset < length
inner = lzma.decompress(fd[at+offset:at+length], format=lzma.FORMAT_ALONE)
assert raw[header:] in inner and uuid.UUID(guid).bytes_le in inner
del inner, raw, payload
kernel = w/'audit/kernel.macho'
summary = json.loads((p/'prelink27/image4/summary.json').read_text())
assert hashlib.sha256(kernel.read_bytes()).hexdigest() == summary['candidate_sha256']
# Compile precisely the parser in the carrier, exposing only its range result.
test = w/'unixnu/drivers/DarwinBoot/Test/test-kernel-parser.py'
context = {'__file__': str(test)}
exec(compile(test.read_text().split('with tempfile.TemporaryDirectory')[0], str(test), 'exec'), context)
with tempfile.TemporaryDirectory() as temporary:
    source = Path(temporary)/'ranges.c'; library = Path(temporary)/'ranges.so'
    source.write_text(context['preamble']+context['types']+context['parser']+'''
int ranges(const UINT8*p,UINTN n,UINT64*out){
 KERNEL k;int status=ParseKernel(p,n,&k);if(status)return status;
 out[0]=k.Base;out[1]=k.End;out[2]=k.Entry;return 0;
}
''')
    subprocess.run(['clang','-shared','-fPIC','-Wall','-Wextra','-Werror',str(source),'-o',str(library)],check=True)
    lib = ctypes.CDLL(str(library)); lib.ranges.argtypes = [ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p]
    blob = kernel.read_bytes(); ranges = (ctypes.c_uint64*3)()
    assert lib.ranges(ctypes.create_string_buffer(blob), len(blob), ranges) == 0
    kernel_base, kernel_end, entry = ranges
adt_bytes = (w/'audit/kernel.macho.adt').stat().st_size
align = lambda value, boundary: (value+boundary-1)&~(boundary-1)
# No ramdisk/trust cache/addon/policy files are configured in this minimal test.
span = kernel_end-kernel_base
used = align(align(align(span,0x4000)+4096+adt_bytes,4096),0x4000)
assert used <= 0x10000000-0x200000
def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)
run('clang','--target=aarch64-none-elf','-c','-DFD_BASE=0x40000000','-DFD_SIZE=0x2000000',
    '-DREQUIRES_KERNEL_HEADER=1',str(mu/'BootShim/BootShim.S'),'-o',str(w/'shim.o'))
run('llvm-objcopy','-O','binary',str(w/'shim.o'),str(w/'shim.bin'))
body = (w/'shim.bin').read_bytes()+fd
(w/'combined.bin').write_bytes(body)
with (w/'Image.lz4').open('wb') as stream:
    run('lz4','-l','-c','-1',str(w/'combined.bin'),stdout=stream)
assert run('lz4','-d','-c',str(w/'Image.lz4'),stdout=subprocess.PIPE).stdout == body
run('python3',str(mu/'Resources/Scripts/mkbootimg.py'),'--kernel',str(w/'Image.lz4'),
    '--header_version','4','--pagesize','4096','--cmdline','','-o',str(w/'boot-image4-test.img'))
boot = (w/'boot-image4-test.img').read_bytes()
assert boot[:8] == b'ANDROID!' and struct.unpack_from('<I',boot,40)[0] == 4
assert struct.unpack_from('<I',boot,12)[0] == 0 and len(boot) < 64*1024**2
preparation = json.loads((w/'preparation.json').read_text())
preparation['previous_kernel_physical_base'] = preparation.pop('kernel_physical_base')
preparation['kernel_allocation'] = 'runtime checked UEFI conventional-memory allocation; not a fixed physical base'
rollback = p/'xnu-adt-phandle-fix/boot-xnu-entry.img'
rollback_sha = 'b4f55025b747568d6b535b93069617e7653f0f14cd23284df18a02b7d50793fd'
assert hashlib.sha256(rollback.read_bytes()).hexdigest() == rollback_sha
output = p/os.environ.get('TB375FC_IMAGE4_EXPORT','image4-first-start'); output.mkdir(exist_ok=True)
shutil.copy2(w/'boot-image4-test.img',output/'boot-image4-test.img')
shutil.copy2(rollback,output/'rollback-unixnu-previous.img')
record = dict(**preparation,boot_bytes=len(boot),boot_sha256=hashlib.sha256(boot).hexdigest(),
              fd_bytes=len(fd),embedded_payload_verified=True,embedded_payload_encoding='LZMA-Alone, decompressed in UniXNURamDiskDxe',production_loader_parser_passed=True,
              kernel_virtual_base=hex(kernel_base),kernel_virtual_end=hex(kernel_end),
              entry=hex(entry),kernel_vm_span=span,window_required=used,window_capacity=0x10000000,
              android_header_version=4,android_ramdisk_bytes=0,boot_partition_capacity=64*1024**2,
              rollback_sha256=rollback_sha,rollback_is='previous UniXNU test image, not stock Android')
if (mu/'Platforms/Lenovo/tb375fcPkg/Library/PrePiLib/FwVol.c').exists():
    assert 'tb375fcPkg/Library/PrePiLib/FwVol.c' in (w/'build.log').read_text(), 'Private PrePi override was not compiled'
    record['prepi_large_ffs_fixed']=True
    record['prepi_source_sha256']=hashlib.sha256((mu/'Platforms/Lenovo/tb375fcPkg/Library/PrePiLib/FwVol.c').read_bytes()).hexdigest()
(output/'qualification.json').write_text(json.dumps(record,indent=2))
shutil.copy2(w/'build-lzma.log',output/'build.log')
(output/'README.md').write_text('''# TB375FC Image4 first-start test

Unflashed host-built test image. Purpose: verify progress past the previous
`image4 interface not available` panic. It is not an iPadOS installation image.
Contains the strict minimal Image4/corecrypto dependency closure, fixed ADT,
and the MAC service-start kernel patch. No OS root volume or Data volume.

`boot-image4-test.img` is intended for boot_a only after explicit test approval.
`rollback-unixnu-previous.img` restores the previous known UniXNU test state;
it does not restore stock Android. See qualification.json for input hashes,
carrier and kernel ranges, parser checks and build-versus-hardware status.
''')
print(json.dumps({key:record[key] for key in ('boot_bytes','boot_sha256','fd_bytes','kernel_vm_span',
      'window_required','window_capacity','device_written','hardware_tested')}),flush=True)
