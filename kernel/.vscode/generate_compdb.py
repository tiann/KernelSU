#!/usr/bin/env python3

from __future__ import print_function, division

import argparse
import fnmatch
import functools
import json
import math
import multiprocessing
import os
import re
import sys


CMD_VAR_RE = re.compile(r'^\s*(?:saved)?cmd_(\S+)\s*:=\s*(.+)\s*$', re.MULTILINE)
SOURCE_VAR_RE = re.compile(r'^\s*source_(\S+)\s*:=\s*(.+)\s*$', re.MULTILINE)


def print_progress_bar(progress):
    progress_bar = '[' + '|' * int(50 * progress) + '-' * int(50 * (1.0 - progress)) + ']'
    print('\r', progress_bar, "{0:.1%}".format(progress), end='\r', file=sys.stderr)


def parse_cmd_file(out_dir, cmdfile_path):
    with open(cmdfile_path, 'r') as cmdfile:
        cmdfile_content = cmdfile.read()

    commands = { match.group(1): match.group(2) for match in CMD_VAR_RE.finditer(cmdfile_content) }
    sources = { match.group(1): match.group(2) for match in SOURCE_VAR_RE.finditer(cmdfile_content) }

    return [{
            'directory': out_dir,
            'command': commands[o_file_name],
            'file': source,
            'output': o_file_name
        } for o_file_name, source in sources.items()]


def gen_compile_commands(cmd_file_search_path, out_dir):
    print("Building *.o.cmd file list...", file=sys.stderr)

    out_dir = os.path.abspath(out_dir)

    if not cmd_file_search_path:
        cmd_file_search_path = [out_dir]

    cmd_files = []
    for search_path in cmd_file_search_path:
        if (os.path.isdir(search_path)):
            for cur_dir, subdir, files in os.walk(search_path):
                cmd_files.extend(os.path.join(cur_dir, cmdfile_name) for cmdfile_name in fnmatch.filter(files, '*.o.cmd'))
        else:
            cmd_files.extend(search_path)

    if not cmd_files:
        print("No *.o.cmd files found in", ", ".join(cmd_file_search_path), file=sys.stderr)
        return

    print("Parsing *.o.cmd files...", file=sys.stderr)

    n_processed = 0
    print_progress_bar(0)

    compdb = []
    pool = multiprocessing.Pool()
    try:
        for compdb_chunk in pool.imap_unordered(functools.partial(parse_cmd_file, out_dir), cmd_files, chunksize=int(math.sqrt(len(cmd_files)))):
            compdb.extend(compdb_chunk)
            n_processed += 1
            print_progress_bar(n_processed / len(cmd_files))

    finally:
        pool.terminate()
        pool.join()

    print(file=sys.stderr)
    print("Writing compile_commands.json...", file=sys.stderr)

    with open('compile_commands.json', 'w') as compdb_file:
        json.dump(compdb, compdb_file, indent=1)


def main():
    cmd_parser = argparse.ArgumentParser()
    cmd_parser.add_argument('-O', '--out-dir', type=str, default=os.getcwd(), help="Build output directory")
    cmd_parser.add_argument('cmd_file_search_path', nargs='*', help="*.cmd file search path")
    gen_compile_commands(**vars(cmd_parser.parse_args()))


if __name__ == '__main__':
    main()
