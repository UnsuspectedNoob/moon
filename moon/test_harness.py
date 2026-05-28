#!/usr/bin/env python3
import sys
import glob
import subprocess

GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
RESET = "\033[0m"


def run_test(filepath):
    expected_outputs = []
    expected_errors = []

    with open(filepath, "r") as f:
        for line in f:
            if "# expect: " in line:
                expected = line.split("# expect: ")[1].strip()
                if expected:
                    expected_outputs.append(expected)
            elif "# expect error: " in line:
                expected_errors.append(line.split("# expect error: ")[1].strip())

    result = subprocess.run(["./moon", filepath], capture_output=True, text=True)

    actual_outputs = [
        line.strip() for line in result.stdout.strip().split("\n") if line.strip()
    ]
    actual_errors = result.stderr.strip()

    passed = True
    failure_reason = ""

    # Check Outputs
    if len(expected_outputs) != len(actual_outputs):
        passed = False
        failure_reason += f"Expected {len(expected_outputs)} output lines, got {len(actual_outputs)}.\n"
        failure_reason += f"  Expected: {expected_outputs}\n  Got: {actual_outputs}\n"
    else:
        for expected, actual in zip(expected_outputs, actual_outputs):
            if expected != actual:
                passed = False
                failure_reason += (
                    f"Output mismatch.\n  Expected: '{expected}'\n  Got: '{actual}'\n"
                )

    # Check Errors
    if expected_errors:
        if result.returncode == 0:
            passed = False
            failure_reason += "Expected an error, but the script exited with code 0.\n"

        for err in expected_errors:
            if err not in actual_errors:
                passed = False
                failure_reason += (
                    f"Error missing from stderr.\n  Expected to find: '{err}'\n"
                )
    else:
        if result.returncode != 0:
            passed = False
            failure_reason += f"Script crashed with exit code {result.returncode}.\n"
            failure_reason += f"  Stderr: {actual_errors}\n"

    return passed, failure_reason


def main():
    if len(sys.argv) > 1:
        test_files = sys.argv[1:]
    else:
        test_files = glob.glob("tests/**/*.moon", recursive=True)

    if not test_files:
        print(f"{YELLOW}No test files found in tests/ directory.{RESET}")
        sys.exit(1)

    print(f"Found {len(test_files)} test files. Running harness...\n")

    passed_count = 0
    failed_tests = []

    for filepath in sorted(test_files):
        passed, reason = run_test(filepath)
        if passed:
            print(f"[{GREEN}PASS{RESET}] {filepath}")
            passed_count += 1
        else:
            print(f"[{RED}FAIL{RESET}] {filepath}")
            failed_tests.append((filepath, reason))

    print("\n" + "=" * 40)
    print(f"Tests Passed: {GREEN}{passed_count}{RESET} / {len(test_files)}")

    if failed_tests:
        print(f"\n{RED}Failures:{RESET}")
        for filepath, reason in failed_tests:
            print(f"--- {filepath} ---")
            print(reason)
        sys.exit(1)
    else:
        print(f"\n{GREEN}All tests passed successfully!{RESET}")
        sys.exit(0)


if __name__ == "__main__":
    main()
