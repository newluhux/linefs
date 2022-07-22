linefs

一个使用FUSE实现的简单文件系统，目的是把文本文件的每一行抽象为一个文件:

```
$ echo "Hello" >> test.txt
$ echo "World" >> test.txt
$ echo "FUSE" >> test.txt
$ echo "I Love 9pfs" >> test.txt
$ export LINEFS_FILE=./test.txt
$ mkdir tmp
$ ./linefs ./tmp/
$ cd tmp
$ ls tmp
1 2 3 4
$ cat 1
Hello
$ cat 2
World
$ cat 3
FUSE
$ cat 4
I Love 9pfs
$ unmount tmp
```
