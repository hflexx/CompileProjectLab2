import os, platform, subprocess, shutil, sys
import difflib

def print_different_lines(file1, file2):
    lines1 = open(file1).readlines()
    lines2 = open(file2).readlines()
    differ = difflib.Differ()
    diff = list(differ.compare(lines1, lines2))
    for line in diff:
        if line.startswith('- ') or line.startswith('+ '):
            print(line)

def score_compiler(arg1):
    record = {}

    is_windows = platform.system() == "Windows"
    
    output_base = "./output/"
    ref_base = "./ref/" + arg1 + "/"

    score = 0
    total = 1  # 只评分一个文件

    if arg1 == "s2":
        for i in ["function"]:
            output_dir = output_base + i + '/'
            ref_dir = ref_base + i + '/'
            if os.path.exists(output_dir):
                files = [f for f in os.listdir(output_dir) if f == "95_float.out"]
                for file in files:
                    cmd = ' '.join(["diff", ref_dir + file, output_dir + file, '-wB'])
                    if is_windows:
                        cmd = cmd.replace('/', '\\')
                    cp = subprocess.run(cmd, shell=True, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
                    if cp.returncode != 0:
                        record[file] = {"retval3": cp.returncode, "err_detail": cp.stderr}
                        print("Expected output:\n", open(ref_dir + file).read())
                        print("Actual output:\n", open(output_dir + file).read())
                        print("Differences:")
                        print_different_lines(ref_dir + file, output_dir + file)
                    else:
                        score += 1
                        record[file] = {"retval3": 0}
                    print(file, record[file])
        print("score:", score, "/", total)
    else:
        print("Unsupported step:", arg1)

    return int(score / total * 100)

if __name__ == "__main__":
    assert(len(sys.argv) == 2)
    score_compiler(sys.argv[1])