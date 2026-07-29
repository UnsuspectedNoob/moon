#!/usr/bin/env python3
import os
import re


def strip_comments_and_strings(text):
    result = []
    i = 0
    n = len(text)
    while i < n:
        if text[i : i + 2] == "/*":
            i += 2
            while i < n and text[i : i + 2] != "*/":
                i += 1
            i += 2
            result.append(" ")
            continue
        elif text[i : i + 2] == "//":
            i += 2
            while i < n and text[i] != "\n":
                i += 1
            result.append("\n")
            continue
        elif text[i] == '"':
            result.append('"')
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == '"':
                    result.append('"')
                    i += 1
                    break
                i += 1
            continue
        elif text[i] == "'":
            result.append("'")
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == "'":
                    result.append("'")
                    i += 1
                    break
                i += 1
            continue

        result.append(text[i])
        i += 1

    return "".join(result)


def extract_signatures(filepath):
    with open(filepath, "r") as f:
        text = f.read()

    text = strip_comments_and_strings(text)

    # Strip preprocessor directives (but NOT DECLARE_ARRAY since it's not starting with #)
    lines = text.split("\n")
    cleaned_lines = []
    in_macro = False
    for line in lines:
        if in_macro:
            if not line.rstrip().endswith("\\"):
                in_macro = False
            continue

        if line.lstrip().startswith("#"):
            if line.rstrip().endswith("\\"):
                in_macro = True
            continue

        cleaned_lines.append(line)

    text = "\n".join(cleaned_lines)

    # Tokenizer state machine
    brace_depth = 0
    paren_depth = 0
    buffer = ""

    structs = []
    functions = []
    globals_vars = []

    i = 0
    n = len(text)

    while i < n:
        c = text[i]

        if c == "{":
            if brace_depth == 0:
                buf_strip = buffer.strip()
                if re.search(r"\b(struct|union|enum)\b", buf_strip):
                    struct_start = i
                    temp_depth = 1
                    i += 1
                    while i < n and temp_depth > 0:
                        if text[i] == "{":
                            temp_depth += 1
                        if text[i] == "}":
                            temp_depth -= 1
                        i += 1
                    while i < n and text[i] != ";":
                        i += 1
                    if i < n:
                        i += 1  # Include semicolon

                    full_struct = buf_strip + " " + text[struct_start:i].strip()
                    full_struct = re.sub(r"\s+", " ", full_struct)
                    structs.append(full_struct)
                    buffer = ""
                    continue
                elif "(" in buf_strip and "=" not in buf_strip:
                    # Function definition
                    sig = re.sub(r"\s+", " ", buf_strip)
                    functions.append(sig + ";")

                    # Skip body
                    temp_depth = 1
                    i += 1
                    while i < n and temp_depth > 0:
                        if text[i] == "{":
                            temp_depth += 1
                        if text[i] == "}":
                            temp_depth -= 1
                        i += 1
                    buffer = ""
                    continue
                else:
                    # Array/Struct initialization `int x[] = { ... }`
                    brace_depth += 1
                    buffer += c
            else:
                brace_depth += 1
                buffer += c

        elif c == "}":
            brace_depth -= 1
            if brace_depth >= 0:
                buffer += c

        elif c == "(":
            if brace_depth == 0:
                paren_depth += 1
            buffer += c

        elif c == ")":
            if brace_depth == 0:
                paren_depth -= 1
            buffer += c

        elif c == ";":
            if brace_depth == 0 and paren_depth == 0:
                buf_strip = buffer.strip()
                if buf_strip:
                    decl_array_match = re.search(
                        r"DECLARE_ARRAY\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)", buf_strip
                    )
                    if decl_array_match:
                        type_name = decl_array_match.group(1).strip()
                        array_name = decl_array_match.group(2).strip()
                        gen_struct = f"typedef struct {{ int capacity; int count; {type_name}* items; }} {array_name};"
                        structs.append(gen_struct)
                        buffer = ""
                        i += 1
                        continue

                    # Ignore standalone empty statements or simple modifiers if they somehow leaked
                    if buf_strip in ['extern "C"', "};"]:
                        buffer = ""
                        i += 1
                        continue

                    if buf_strip.startswith("typedef") and not re.search(
                        r"\b(struct|union|enum)\b", buf_strip
                    ):
                        globals_vars.append(re.sub(r"\s+", " ", buf_strip) + ";")
                    elif "(" in buf_strip and not buf_strip.startswith("typedef"):
                        # Function prototype or function pointer
                        functions.append(re.sub(r"\s+", " ", buf_strip) + ";")
                    else:
                        # Global variable
                        globals_vars.append(re.sub(r"\s+", " ", buf_strip) + ";")
                buffer = ""
            else:
                buffer += c

        else:
            buffer += c

        i += 1

    return structs, functions, globals_vars


def main():
    import glob

    files = glob.glob("src/**/*.h", recursive=True) + glob.glob(
        "src/**/*.c", recursive=True
    )

    filtered_files = []
    for f in files:
        basename = os.path.basename(f).lower()
        if "lsp" in basename or "cjson" in basename or "main" in basename:
            continue
        filtered_files.append(f)

    results_by_file = {}
    for filepath in sorted(filtered_files):
        s, f, g = extract_signatures(filepath)
        if s or f or g:
            results_by_file[filepath] = {
                "structs": sorted(list(set(s))),
                "functions": sorted(list(set(f))),
                "globals": sorted(list(set(g))),
            }

    with open("signatures_output.txt", "w") as out:
        for filepath, data in results_by_file.items():
            basename = os.path.basename(filepath).upper()
            out.write(f"=== {basename} ===\n")

            if data["structs"]:
                out.write("  --- STRUCTS & ENUMS ---\n")
                for x in data["structs"]:
                    out.write(f"    {x}\n")

            if data["functions"]:
                out.write("  --- FUNCTIONS ---\n")
                for x in data["functions"]:
                    out.write(f"    {x}\n")

            if data["globals"]:
                out.write("  --- GLOBALS & TYPEDEFS ---\n")
                for x in data["globals"]:
                    out.write(f"    {x}\n")

            out.write("\n")

    print("Signatures written to signatures_output.txt (Grouped by file)")


if __name__ == "__main__":
    main()
