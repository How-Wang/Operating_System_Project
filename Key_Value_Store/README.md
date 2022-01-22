# Simple Key-value Store

## Directories
- /server ->	server program related sources
- /client ->	client program related sources
- /common ->	common inclusions
- /util ->	common utilization
- /build ->	target build directory

## Building the Project
Code out your `/server/server.c` and `/client/client.c`, then
```shell
$ make
```
Test your `/build/server` and `build/client`.
## Demo picture
![](https://i.imgur.com/7xcJYQY.png)

## Implementations
### Multi-threading design pattern
![image](https://user-images.githubusercontent.com/62500402/143755173-137cbd45-0b62-4efa-ac81-fa3d0334e1a8.png)

### Data structure implementation
我直接使用**檔案**的建立，
模式為 ```key.txt``` 內存```value```
-  ```SET``` 一個新的 pair 就建立新檔案
-  ```GET``` 為讀檔案
-  ```DELETE``` 為刪除檔案

## References
* [POSIX thread man pages](https://man7.org/linux/man-pages/man7/pthreads.7.html)
* [socket man pages](https://linux.die.net/man/7/socket)

