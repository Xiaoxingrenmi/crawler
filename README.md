# Naive Async Crawler

> by BOT Man & ZhangHan, 2018

## Requirements

``` bash
# libevent
sudo apt-get install libevent-dev

# Visual Studio Linux Development
sudo apt-get install openssh-server g++ gdb gdbserver
```

## Compile

``` bash
clang crawler/*.c -Wall -levent -o crawler.out
```

## Test Website

``` bash
# nginx
sudo apt-get install nginx
sudo nginx
sudo cp -r www/* /var/www/html/

./crawler.out localhost/
```
