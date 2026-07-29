#!/usr/bin/env python3
import sys
import re
import os

def parse_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    filename = os.path.basename(filepath)
    
    # Simple regex to find structs (captures struct Name { ... }; and typedef struct { ... } Name;)
    structs = []
    # Match: typedef struct { ... } Name;
    typedef_struct_pattern = re.compile(r'typedef\s+struct\s*(?:[a-zA-Z0-9_]+\s*)?\{([^}]*)\}\s*([a-zA-Z0-9_]+)\s*;', re.MULTILINE)
    for match in typedef_struct_pattern.finditer(content):
        fields = [f.strip() for f in match.group(1).split(';') if f.strip()]
        name = match.group(2)
        structs.append((name, fields))

    # Match: struct Name { ... };
    struct_pattern = re.compile(r'struct\s+([a-zA-Z0-9_]+)\s*\{([^}]*)\}\s*;', re.MULTILINE)
    for match in struct_pattern.finditer(content):
        name = match.group(1)
        fields = [f.strip() for f in match.group(2).split(';') if f.strip()]
        structs.append((name, fields))
        
    # Variables (Global or Static)
    variables = []
    var_pattern = re.compile(r'^(static\s+)?[a-zA-Z_][a-zA-Z0-9_*\s]+\s+([a-zA-Z_][a-zA-Z0-9_]*)(?:\[[^\]]*\])?\s*(?:=[^;]+)?;', re.MULTILINE)
    for match in var_pattern.finditer(content):
        var_decl = match.group(0).strip()
        # Ignore forward declarations of functions (they have parentheses)
        if '(' not in var_decl and 'typedef' not in var_decl and 'return' not in var_decl:
            variables.append(var_decl)

    # Functions
    functions = []
    # Match standard C functions: static void name(args) { or Node *name(args) {
    func_pattern = re.compile(r'^(?:static\s+)?(?:inline\s+)?[a-zA-Z_][a-zA-Z0-9_*\s]*\s*\**\s*[a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*\)\s*\{', re.MULTILINE)
    for match in func_pattern.finditer(content):
        signature = match.group(0).strip().rstrip('{').strip()
        functions.append(signature)
        
    # Header file functions (ending with ;)
    if filepath.endswith('.h'):
        header_func_pattern = re.compile(r'^(?:static\s+)?(?:inline\s+)?[a-zA-Z_][a-zA-Z0-9_*\s]*\s*\**\s*[a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*\)\s*;', re.MULTILINE)
        for match in header_func_pattern.finditer(content):
            signature = match.group(0).strip().rstrip(';').strip()
            # Ignore typedef function pointers
            if 'typedef' not in signature:
                functions.append(signature)

    # Output Markdown
    print(f"# Documentation: `{filename}`\n")
    print("## Overview")
    print("- **Purpose**: [INSERT MODULE PURPOSE HERE]\n")
    
    if structs:
        print("## Structs")
        for name, fields in structs:
            print(f"### `{name}`")
            # Format fields a bit nicer
            clean_fields = [f.replace('\n', ' ').strip() for f in fields]
            print(f"- **Fields**: `{', '.join(clean_fields)}`")
            print("- **Description**: [INSERT DESCRIPTION HERE]\n")
            
    if variables:
        print("## Global/Static Variables")
        for var in variables:
            print(f"### `{var}`")
            print("- **Description**: [INSERT DESCRIPTION HERE]\n")

    if functions:
        print("## Functions")
        for func in functions:
            print(f"### `{func}`")
            print("- **Description**: [INSERT DESCRIPTION HERE]\n")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: extract_docs.py <file.c>")
        sys.exit(1)
    parse_file(sys.argv[1])
