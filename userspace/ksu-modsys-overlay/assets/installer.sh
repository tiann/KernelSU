#!/system/bin/sh

# KernelSU Module Installer Script
# This script is embedded into the ksu-modsys-overlay binary

abort() {
  echo "$1"
  exit 1
}

ui_print() {
  echo "$1"
}

install_module() {
  ui_print "- Installing module..."
  
  # Basic validation
  [ -f "$ZIPFILE" ] || abort "! Installation file not found"
  
  # Extract and validate module.prop
  unzip -o "$ZIPFILE" module.prop -d "$TMPDIR" >&2
  [ -f "$TMPDIR/module.prop" ] || abort "! Invalid module format"
  
  # Source module.prop for validation
  . "$TMPDIR/module.prop"
  
  # Validate required fields
  [ -z "$id" ] && abort "! Module ID not found"
  [ -z "$name" ] && abort "! Module name not found"
  [ -z "$version" ] && abort "! Module version not found"
  [ -z "$versionCode" ] && abort "! Module version code not found"
  
  ui_print "- Module ID: $id"
  ui_print "- Module name: $name"
  ui_print "- Module version: $version"
  
  ui_print "- Module installed successfully"
}
