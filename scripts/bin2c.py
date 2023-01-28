#!/usr/bin/python3

import argparse
import os
import re

line_size = 80


def bin2c(filename, varname='data'):
    if not os.path.isfile(filename):
        print('File "%s" is not found!' % filename)
        return ''
    if not re.match('[a-zA-Z_][a-zA-Z0-9_]*', varname):
        print('Invalid variable name "%s"' % varname)
        return
    with open(filename, 'rb') as in_file:
        data = in_file.read()
    # limit the line length
    byte_len = 6  # '0x00, '
    out = 'unsigned int %s_size = %d;\n' \
          'const char %s[%d] = {\n' % (varname, len(data), varname, len(data))
    line = ''
    for byte in data:
        line += '0x%02x, ' % byte
        if len(line) + 4 + byte_len >= line_size:
            out += ' ' * 4 + line + '\n'
            line = ''
    # add the last line
    if len(line) + 4 + byte_len < line_size:
        out += ' ' * 4 + line + '\n'
    # strip the last comma
    out = out.rstrip(', \n') + '\n'
    out += '};'
    return out


def main():
    """ Main func """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'filename', help='filename to convert to C array')
    parser.add_argument(
        'varname', nargs='?', help='variable name', default='data')
    args = parser.parse_args()
    # print out the data
    print(bin2c(args.filename, args.varname))


if __name__ == '__main__':
    main()
