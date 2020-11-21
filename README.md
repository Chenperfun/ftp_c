FTP 协议——服务器-客户端的实现
===========
该项目是一个轻量级的文件传输程序的简单实现。程序提供对用户身份的本地认证、列出远程服务器端文件列表，并传输。

自定义客户端和服务器程序，这些程序提供对用户进行身份验证、列出远程文件和检索远程文件的功能。

Simple implementation of a file transfer program. It includes custom client and server programs that provide functionality to authenticate a user, list remote files, and retrieve remote files.

# FTP简介

FTP（File Transfer Protocol，文件传输协议） 是 TCP/IP 协议组中的协议之一。FTP协议包括两个组成部分，其一为FTP服务器，其二为FTP客户端。其中FTP服务器用来存储文件，用户可以使用FTP客户端通过FTP协议访问位于FTP服务器上的资源。在开发网站的时候，通常利用FTP协议把网页或程序传到Web服务器上。此外，由于FTP传输效率非常高，在网络上传输大的文件时，一般也采用该协议。

默认情况下FTP协议使用TCP端口中的 20和21这两个端口，其中20用于传输数据，21用于传输控制信息。但是，是否使用20作为传输数据的端口与FTP使用的传输模式有关，如果采用主动模式，那么数据传输端口就是20；如果采用被动模式，则具体最终使用哪个端口要服务器端和客户端协商决定。



## 结构

	ftp/
		client/
			ftclient.c
			ftclient.h
			makefile
		common/
			common.c
			common.h
		server/
			ftserve.c
			ftserve.h
			makefile
			.auth

## 使用

To compile and link ftserve:

```
	$ cd server/
	$ make
```

To compile and link ftclient:
```
	$ cd client/
	$ make
```

To run ftserve:
```
	$ server/ftserve PORTNO
```

To run ftclient:
```
	$ client/ftclient HOSTNAME PORTNO

	Commands:
		list
		get <filename>
		quit
```

Available commands:
```
list            - retrieve list of files in the current remote directory
get <filename>  - get the specified file
quit            - end the ftp session
```

Logging In:
```
	Name: anonymous
	Password: [empty]
```



## FTP工作模式

FTP的完整工作有2个TCP连接，分别用于命令传输和数据传输(文件传输)。其分开为2个连接主要就是为了防止传输二进制文件破坏了命令连接的终端，可以在命令连接中指定数据传输的模式，以此来降低程序开发的复杂性。