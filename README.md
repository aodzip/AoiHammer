# AoiHammer
用于64bit长度汉明距离搜索设计的多线程全内存搜索引擎
## 编译
标准make编译，依赖libpthread
## 启动参数

AoiHammer: Hamming distance search engine

Usage: ./aoihammer \[OPTIONS\]

         -d Running in daemon mode.
         
         -l [ADDR] Server listen address. Default: 127.0.0.1
         
         -p [PORT] Server listen port. Default: 8023
         
         -f [FILENAME] Persistence file path. Default: ./hashs.export
         
 -d 为Daemon后台运行，程序运行日志将写入syslog
 
 -l 为服务监听IP
 
 -p 为服务监听端口
 
 -f 持久化文件位置，该持久化文件格式兼容mongoDB表导出格式

## 搜索引擎指令
搜索引擎指令为纯tcp socket ASCII模式，使用telnet可以进行调试
### A 增加索引指令
#### 用法
A \[id\] \[hash\] 其中id大于0，该指令不会对id进行查重，该指令会更新持久化文件
#### 返回值
ok 数据插入成功
 
error 内存满或id非法
### S 搜索指令
#### 用法
S \[最大距离阈值\] \[返回个数\] 
#### 返回值
\[id\]:\[距离\],...
### Q 服务端停止指令
#### 用法
Q 
#### 返回值
shutdown

## 持久化文件
该文件会在增加
### 格式
兼容mongodb export格式
### 例子
{"_id":%u,"hash":{"$numberLong":"%ld"}}\n
