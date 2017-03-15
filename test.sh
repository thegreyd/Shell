for testfile in $(ls tests/test*.txt) 
do
	echo "running $testfile"
	./ush < "$testfile" > tests/out_ush.txt 2>&1
	csh < "$testfile" > tests/out_csh.txt 2>&1
	diff -u tests/out_ush.txt tests/out_csh.txt
done