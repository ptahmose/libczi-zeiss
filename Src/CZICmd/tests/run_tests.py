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


# Press Shift+F10 to execute it or replace it with your code.
# Press Double Shift to search everywhere for classes, files, tool windows, actions, and settings.

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
    cmdlineargs.append(test_case['output'])
    cmdlineargs.append("--calc-hash")
    if verbosity > 2:
        print(cmdlineargs)
    p = subprocess.Popen(cmdlineargs, stdout=subprocess.PIPE, universal_newlines=True)
    (md5sum_output, err) = p.communicate()
    #print("OUTPUT:" + md5sum_output)
    reResult = re.search("^hash of result: ([0-9a-fA-F]{32})", md5sum_output, re.MULTILINE)
    chk_sum = reResult.group(1)
    #    print( "chksum:"+chksum+"   testCase:"+testCase['md5sum'])
    if (chk_sum.upper() == test_case['md5sum'].upper()):
        #print(" -> OK")
        return True
    else:
        #print("-> FAILURE")
        if verbosity > 0:
            print("is:" + chk_sum.upper() + " expected:" + test_case['md5sum'].upper())
        return False


def print_hi(name):
    # Use a breakpoint in the code line below to debug your script.
    print(f'Hi, {name}')  # Press Ctrl+F8 to toggle the breakpoint.


# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    print_hi('PyCharm')

    testCases = readTestCases(test_cases_filename)

    i = 1
    for test_case in testCases:
        result = run_test_cases(test_case)
        if result:
            print(str(i) + ": ** OK **")
        else:
            print(str(i) + ": ** FAILURE **")
        i = i + 1
# See PyCharm help at https://www.jetbrains.com/help/pycharm/
