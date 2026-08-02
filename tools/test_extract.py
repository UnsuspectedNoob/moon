import os
import re
import glob

# Try to parse chunk.h and chunk.c
def parse_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # Remove block comments
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    # Remove line comments
    content = re.sub(r'//.*', '', content)

    # Structs
    struct_pattern = re.compile(r'(?:typedef\s+)?struct\s*([a-zA-Z_][a-zA-Z0-9_]*\s*)?\{([^}]*)\}\s*([a-zA-Z_][a-zA-Z0-9_]*\s*)?;', re.MULTILINE)
    
    print(f"--- Structs in {filepath} ---")
    for match in struct_pattern.finditer(content):
        name = match.group(3) or match.group(1)
        name = name.strip() if name else "ANONYMOUS"
        body = match.group(2).strip()
        print(f"Struct: {name}")
        for line in body.split('\n'):
            line = line.strip()
            if line:
                print(f"  {line}")

    # Functions (heuristics)
    # Match return type, name, args, then {
    func_pattern = re.compile(r'^((?:static\s+)?[a-zA-Z_][a-zA-Z0-9_\s\*\*]+)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\)\s*\{', re.MULTILINE)
    print(f"--- Functions in {filepath} ---")
    for match in func_pattern.finditer(content):
        ret = match.group(1).strip()
        name = match.group(2).strip()
        args = match.group(3).strip()
        # Clean up whitespace
        ret = re.sub(r'\s+', ' ', ret)
        args = re.sub(r'\s+', ' ', args)
        print(f"{ret} {name}({args});")

parse_file('src/chunk.h')
parse_file('src/chunk.c')
