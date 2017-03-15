from __future__ import absolute_import, print_function,division
from subprocess import PIPE, Popen
import time

testfiles = ["basic_stdin.txt", "basic_stdout.txt", "basic_stderr.txt"]
program1 = "ush -n"
program2 = "csh"
program3 = "diff -u"
output1 = "out1.txt"
output2 = "out2.txt"

#command = "{} {} {}".format(program3, testfiles[0], testfiles[1])
#c = Popen(command, stdout=PIPE, stderr=PIPE, shell=True).communicate()
#print(command)
#print(c[0].decode("utf-8"),end ='')
#print(c[1].decode("utf-8"),end ='')
#print("{}: {}".format(command,c[0]))


for testfile in testfiles:
	command = "{} < {} > {} 2>&1".format(program1, testfile, output1)
	print("running {}".format(command))
	Popen(command, shell=True)
	command = "{} < {} > {} 2>&1".format(program2, testfile, output2)
	print("running {}".format(command))
	Popen(command, shell=True)
	time.sleep(2)
	command = "{} {} {}".format(program3, output1, output2)
	c = Popen(command, stdout=PIPE, stderr=PIPE, shell=True).communicate()
	print("running {}".format(command))
	print(c[0].decode("utf-8"),end ='')
	print(c[1].decode("utf-8"),end ='')