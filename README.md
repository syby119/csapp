## CSAPP

#### Introduction
Here are some materials for self-study students to the course **CMU 15-213: CSAPP**.

You can find the online course on [Bilibili](https://www.bilibili.com/video/BV1iW411d7hd), although the Chinese translation is not so satisfying for some video parts.

Besides, I've worked out some of the labs for your reference if you meet any problem.

Get your hands dirty. Good luck :-)

#### Notice
The lab directory contains the raw materials of seven labs from the official [course website](http://csapp.cs.cmu.edu/3e/labs.html). They may not run correctly on your computer. If you are a Windows user and use WSL 2 with Ubuntu, and use python3 instead of python2, just copy the files except the solution into your working directory.

Moreover, there are some useful toolkits and extra materials in the labref. I hope they can help you finish your own tasks.

#### Lab reference
The following code is compiled and run at on Windows 10 in WSL 2 with Ubuntu 20.04 LTS. There is no guarantee for the validness on the other Operating Systems.
##### datalab
```shell
cd labref/datalab/datalab-handout
make
./driver.pl
```

##### bomblab
```shell
cd labref/bomblab/bomb
./bomb ./answer.txt
```

##### attacklab
```shell
cd labref/attacklab/target1
./run_answer.sh
```

##### shelllab
```shell
cd labref/shelllab/shlab-handout
make
python3 autograde.py
```

##### cachelab
```shell
cd labref/cachelab/cachelab-handout
make
python3 driver.py
```

##### malloclab
```shell
cd labref/malloclab/malloclab-handout
make
./mdriver -v
```

##### proxylab
```shell
cd labref/proxylab/proxylab-handout
make
./driver.sh
```