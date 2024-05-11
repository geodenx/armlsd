# ARM Linux Serial Downloader

ARM Linux Serial Downloader is similar to ARMWSD.exe (ARM Windows Serial Downloader) provided by Analog Devices.
It can execute write, erase, verify and run commands in ADuC702x Serial Download Protocol.  
It is tested with ADuC2076 attached to "DesignWave 2006 March" published by CQ publishing.

『DesignWave 2006年3月号』付録 Analog Devices ADuC7026 向け Linux 版シリアルダウンローダ．
Analog Devices からは Windows 版シリアルダウンローダ ARMWSD [analog.com] が提供されいるが，Linux はサポートされていないので作成した．

## Usage
```
$ make # Linux
$ make CFLAGS=-DFREEBSD # FreeBSD
$ ./armlsd
usage: ./armlsd [-e|-w|-v|-p port] [file]
        -e              erase the entire user code space
        -p port         default: /dev/ttyS0
        -w input.hex    write
        -v input.hex    verify
$ ./armwsd sample.hex (In advance, switch CPU to boot-mode.)
```

## References
1. Analog Devices AN-724: ADuC702x Serial Download Protocol
2. Serial Programming HOWTO [tldp.org]
