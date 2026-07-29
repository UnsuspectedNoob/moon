import os
import sys
from lark import Lark

def main():
    grammar_path = os.path.join(os.path.dirname(__file__), "..", "ebnf", "moon.lark")
    tests_dir = os.path.join(os.path.dirname(__file__), "..", "tests")
    
    print(f"Loading grammar from {grammar_path}...")
    
    # We load all the individual files manually to concatenate them,
    # as Lark's %import system expects a slightly different setup for local files
    # if not properly configured with import_paths.
    grammar = ""
    ebnf_dir = os.path.dirname(grammar_path)
    for file in ["1_lexical.lark", "2_expressions.lark", "3_statements.lark", "4_phrasal_trie.lark", "moon.lark"]:
        with open(os.path.join(ebnf_dir, file), "r") as f:
            content = f.read()
            # Strip out %import statements since we're concatenating
            content = "\n".join(line for line in content.split("\n") if not line.startswith("%import"))
            grammar += content + "\n\n"
            
    print("Compiling grammar (this will check for Shift/Reduce and ambiguity)...")
    try:
        # We use 'lalr' to strictly prove if a grammar is deterministic.
        # This will intentionally throw a fatal Reduce/Reduce collision 
        # to mathematically prove the necessity of the Phrasal Trie!
        parser = Lark(grammar, start="start", parser="lalr", lexer="contextual")
        print("Grammar compiled successfully with LALR(1)! No fatal ambiguities.\n")
    except Exception as e:
        print(f"Grammar Compilation Failed:\n{e}")
        sys.exit(1)

    print("Verifying tests...")
    passed = 0
    failed = 0
    
    for root, _, files in os.walk(tests_dir):
        for file in files:
            if file.endswith(".moon"):
                filepath = os.path.join(root, file)
                with open(filepath, "r") as f:
                    code = f.read()
                
                try:
                    # Let Lark parse the code
                    parser.parse(code)
                    print(f"[PASS] {os.path.relpath(filepath)}")
                    passed += 1
                except Exception as e:
                    print(f"[FAIL] {os.path.relpath(filepath)}")
                    if failed == 0:
                        print(e)
                    failed += 1

    print(f"\nResults: {passed} Passed, {failed} Failed")

if __name__ == "__main__":
    main()
