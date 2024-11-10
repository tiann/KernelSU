from xml.dom.minidom import parse
import xml.dom.minidom
import sys


DOMTree = xml.dom.minidom.parse(sys.argv[1])
symbols = DOMTree.getElementsByTagName("elf-symbol")
print("[abi_symbol_list]")
for symbol in symbols:
    print("  " + symbol.getAttribute("name"))
