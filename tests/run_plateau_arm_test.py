"""Execute the real compiled ARM controller tests using Unicorn (VFP enabled).
Usage: python run_plateau_arm_test.py path/to/plateau_test.elf
Requires unicorn and pyelftools; no physical IO or device connection.
"""
import sys
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC

with open(sys.argv[1], 'rb') as f:
    elf=ELFFile(f); entry=elf.header['e_entry']; segments=[]
    for seg in elf.iter_segments():
        if seg['p_type']=='PT_LOAD': segments.append((seg['p_vaddr'],seg['p_memsz'],seg.data()))
mu=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
pages=set()
for address,size,data in segments:
    for page in range(address&~4095,(address+size+4095)&~4095,4096):
        if page not in pages:mu.mem_map(page,4096);pages.add(page)
    mu.mem_write(address,data)
mu.mem_map(0x20000000,0x20000);mu.reg_write(UC_ARM_REG_SP,0x2001fff0)
stop=0x40000000;mu.mem_map(stop,4096);mu.reg_write(UC_ARM_REG_LR,stop|1)
mu.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);mu.reg_write(UC_ARM_REG_FPEXC,0x40000000)
mu.emu_start(entry|1,stop,count=100000000)
if mu.reg_read(UC_ARM_REG_PC)!=stop:raise SystemExit('Test did not return (instruction budget exceeded)')
result=mu.reg_read(UC_ARM_REG_R0)
print('ARM Plateau controller tests:', 'PASS' if result==0 else 'FAIL at C++ line '+str(result))
sys.exit(0 if result==0 else 1)
