#!/usr/bin/env python3
import sys
import re
import subprocess


def get_func_and_line(addr):
    name = subprocess.check_output(f'llvm-addr2line-15 --functions -e {elf_path} {addr}', shell=True).decode('utf-8')
    func, file = name.splitlines()
    return f' {func} ({file})'

def process_input(elf_path):
    for line in sys.stdin:
        line = line[:-1]
        if line.startswith('[-] RIP=ffffffff8'):
            line += get_func_and_line(line[8:])
        if line.startswith('[-] \tffffffff8'):
            line += get_func_and_line(line[5:])

        print(line)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py <path_to_elf>")
        sys.exit(1)

    elf_path = sys.argv[1]
    process_input(elf_path)