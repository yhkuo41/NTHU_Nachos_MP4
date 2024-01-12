../build.linux/nachos -f
../build.linux/nachos -mkdir /t0
../build.linux/nachos -mkdir /t1
../build.linux/nachos -cp num_100.txt /t0/f1
../build.linux/nachos -cp num_1000.txt /t0/f2
../build.linux/nachos -mkdir /t0/aa
../build.linux/nachos -cp num_100.txt /t0/aa/f1
../build.linux/nachos -cp 64MB.txt /t0/aa/64MB
../build.linux/nachos -l /
echo ===================
../build.linux/nachos -lr /

../build.linux/nachos -h /t0/aa/64MB
# bash FS_fileheader.sh > FS_fileheader_cout0.log 2> FS_fileheader_cerr0.log