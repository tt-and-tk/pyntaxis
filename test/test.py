from glob import glob
import subprocess

for asm in glob(".\\asm\\*.asm"):
    sv = asm.replace("\\asm\\", "\\bin\\")
    sv = sv.replace(".asm", ".sv")
    # print(asm, sv)
    subprocess.call(["..\\asm2bin.exe", asm, "-o", sv])
    # break
