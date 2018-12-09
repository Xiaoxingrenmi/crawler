# Naive Async Crawler

> by BOT Man & ZhangHan, 2018

## Features

- use [libevent](https://libevent.org) to process async IO
- use [bloom filter](https://en.wikipedia.org/wiki/Bloom_filter) to implement url set
- use [deterministic finite automaton](https://en.wikipedia.org/wiki/Deterministic_finite_automaton) to parse `<a>` tag urls inside html

## Requirements

``` bash
# libevent
sudo apt-get install libevent-dev

# Visual Studio Linux Development
sudo apt-get install openssh-server g++ gdb gdbserver
sudo service ssh start
```

## Compile

``` bash
clang++ crawler/*.c crawler/*.cpp -Wall -levent -o crawler.out
```

## Test Website

``` bash
# nginx
sudo apt-get install nginx
sudo nginx
sudo cp -r www/* /var/www/html/

./crawler.out localhost/
```

## Internals

### Trans-State Diagram

```
                     DoInit   DoConn   DoSend   DoRecv
                        \        \        \        \
 (CreateState) --> Init --> Conn --> Send --> Recv --> Succ --:
                    :        :        :        :              :--> (FreeState)
                    :--------:--------:--------:-----> Fail --:
```

### Trans-State Table

Old State | New State | Old Event | New Event | Old Buffer | New Buffer
---|---|---|---|---|---
Init | Conn | NULL | EV_WRITE + DoConn | NULL | NULL
Conn | Send | EV_WRITE + DoConn | EV_WRITE + DoSend | NULL | Send Buffer
Send | Recv | EV_WRITE + DoSend | EV_READ + DoRecv | Send Buffer | NULL
Recv | Succ | EV_READ + DoRecv | NULL | Recv Buffer | NULL
? | Fail | ? | NULL | ? | NULL
