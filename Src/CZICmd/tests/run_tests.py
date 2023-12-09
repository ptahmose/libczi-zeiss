from typing import Any

import sys
import fileinput
import subprocess
import re
import argparse

czicmd_executable = r"D:\dev\Github\libczi-zeiss-ptahmose\out\build\x64-Debug\Src\CZICmd\CZIcmd.exe"
test_cases_filename = "testcases.txt"
verbosity = 1

def readTestCases(filename):
    testCases = []
    testCaseNo = 0
    testCasePart = 0
    for line in fileinput.input(filename):
        line = line.strip()
        if line and not line.startswith("#"):
            if testCasePart == 0:
                testCases.append({'args': line})
                testCasePart += 1
            elif testCasePart == 1:
                testCases[testCaseNo].update({'input': line})
                testCasePart += 1
            elif testCasePart == 2:
                testCases[testCaseNo].update({'output': line})
                testCasePart += 1
            elif testCasePart == 3:
                testCases[testCaseNo].update({'md5sum': line})
                testCasePart = 0
                testCaseNo += 1

    return testCases

def run_test_cases(test_case):
    cmdlineargs = [czicmd_executable]
    test = test_case['args']
    test2 = test.split(' ')
    cmdlineargs.extend(test_case['args'].split())
    cmdlineargs.append("--source")
    cmdlineargs.append(test_case['input'])
    cmdlineargs.append("--source-stream-class")
    cmdlineargs.append("curl_http_inputstream")
    cmdlineargs.append("--output")
    cmdlineargs.append(".")
    cmdlineargs.append("--calc-hash")
    if verbosity > 2:
        print(cmdlineargs)
    p = subprocess.Popen(cmdlineargs, stdout=subprocess.PIPE, universal_newlines=True)
    (md5sum_output, err) = p.communicate()
    reResult = re.search("^hash of result: ([0-9a-fA-F]{32})", md5sum_output, re.MULTILINE)
    chk_sum = reResult.group(1)
    if (chk_sum.upper() == test_case['md5sum'].upper()):
        return True
    else:
        if verbosity > 0:
            print("(hash is:" + chk_sum.upper() + " expected:" + test_case['md5sum'].upper() + ")")
        return False

def verbosity_level(string):
    # Map text labels to numbers
    levels = {'none': 0, 'normal': 1, 'chatty': 2, 'debug': 3}
    
    # Check if the string is one of the text labels
    if string.lower() in levels:
        return levels[string.lower()]
    
    # If not, try to convert it to an integer and check if it's in the valid range
    try:
        value = int(string)
        if 0 <= value <= 3:
            return value
        else:
            raise argparse.ArgumentTypeError("Verbosity level must be between 0 and 3")
    except ValueError:
        raise argparse.ArgumentTypeError(f"Invalid verbosity level: {string}")


if __name__ == '__main__':
     # Create the parser
    parser = argparse.ArgumentParser(description='Run CZICmd with a list of parameters, and check the result against a known hash.')

     # Add arguments with short options
    parser.add_argument('-c', '--czicmd-executable', type=str, required=False,
                        help='Path to the czicmd executable')
    parser.add_argument('-t', '--testcases', type=str, required=False,
                        help='Path to the testcases file')
    parser.add_argument('-v', '--verbosity', type=verbosity_level, default='normal', required=False,
                        help='Set the verbosity level (0=none, 1=normal, 2=chatty, 3=debug)')

    # Parse the arguments
    args = parser.parse_args()

    if args.czicmd_executable:
      czicmd_executable = args.czicmd_executable

    if args.testcases:
      test_cases_filename = args.testcases

    verbosity = args.verbosity

    testCases = readTestCases(test_cases_filename)

    i = 1
    error_count = 0
    for test_case in testCases:
        print("Testcase #" + str(i) + ": ", end="")
        result = run_test_cases(test_case)
        if result:
            print("** OK **")
        else:
            print("** FAILURE **")
            error_count += 1
        i += 1

    print()
    if error_count > 0:
        print("There were " + str(error_count) + " error(s).")
        sys.exit(1)
    else:
        print("All tests passed.")
        sys.exit(0)
