from typing import Any

import sys
import fileinput
import subprocess
import re
import argparse

czicmd_executable = r"D:\dev\Github\libczi-zeiss-ptahmose\out\build\x64-Debug\Src\CZICmd\CZIcmd.exe"
destination_folder = "N:/Test"
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
    #cmdlineargs.append(test_case['output'])
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

def parse_commandline_arguments:
    parser = argparse.ArgumentParser(description='Run CZIcmd testcases.')
    parser.add_argument('--czicmd', dest='czicmd_executable', default=czicmd_executable, help='path to CZIcmd executable')
    parser.add_argument('--testcases', dest='test_cases_filename', default=test_cases_filename, help='path to testcases file')
    parser.add_argument('--verbosity', dest='verbosity', default=verbosity, help='verbosity level')
    parser.add_argument('--destination', dest='destination_folder', default=destination_folder, help='destination folder')
    args = parser.parse_args()
    return args

if __name__ == '__main__':
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
