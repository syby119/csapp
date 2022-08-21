# this is a nonofficial autograder written by yy,
# run in linux with python 3:
# python3 autograde.py

import os
import subprocess


def get_program_result(cmd):
    pipe = subprocess.Popen(
            cmd, 
            shell=True, 
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)

    (ret, _) = pipe.communicate()
    ret = str(ret, encoding='utf-8')
    ret = ret.splitlines()

    return ret


def run_autograde(trace_file):
    cmd = "./sdriver.pl -t " + trace_file

    ans = get_program_result(cmd + " -s ./tsh")
    ref = get_program_result(cmd + " -s ./tshref")

    if (len(ans) != len(ref)):
        print("outputs has different lines: ")
        print("+ ans: %d lines" + len(ans))
        print("+ ref: %d lines" + len(ref))

    success = True
    ps_enable = False
    ps_out_lines = 0
    for lid in range(len(ans)):
        if "tsh> /bin/ps a" in ans[lid]:
            if ans[lid] != ref[lid]:
                success = False
                break
            else:
                ps_enable = True
                continue
        
        if ps_enable:
            if ans[lid].startswith("tsh> ") and ps_out_lines > 0:
                ps_enable = False
                ps_out_lines = 0
            else:
                if ref[lid].startswith("tsh> ") and ps_out_lines > 0:
                    success = False
                    break
                ps_out_lines += 1
                continue

        i, j = 0, 0
        alen, rlen = len(ans[lid]), len(ref[lid])
        while i < alen and j < rlen:
            if ans[lid][i] == '(':
                while i < alen and ans[lid][i] != ')':
                    i += 1
            if ref[lid][j] == '(':
                while j < rlen and ref[lid][j] != ')':
                    j += 1
            if i < alen and j < rlen and ans[lid][i] != ref[lid][j]:
                success = False
                break
            i += 1
            j += 1

        if i != alen and j != rlen:
            success = False
            break

    if success:
        print(trace_file + " pass")
    else:
        print("different result at %s(%d)" % (trace_file, lid))
        print("+ ans: " + ans[lid])
        print("+ ref: " + ref[lid])

    return success


if __name__ == "__main__":
    for filepath, dirname, filenames in os.walk("./"):
        for filename in filenames:
            if filename.startswith("trace") and filename.endswith(".txt"):
                run_autograde(os.path.join(filepath, filename))