# ARM Linux Serial Downloader

『DesignWave 2006年3月号』付録 Analog Devices ADuC7026 向け Linux 版シリアルダウンローダ．Analog Devices からは Windows 版シリアルダウンローダ ARMWSD [analog.com] が提供されいるが，Linux はサポートされていないので作成した．

ARM Linux Serial Downloader having the same functions as ARMWSD.exe (ARM Windows Serial Downloader) provided by Analog Devices. It can execute write, erase, verify and run commands of ADuC702x Serial Download Protocol. My motivation is that ADuC2076 is attached to "DesignWave 2006 March" CQ publishing, which doesn't provide Linux downloaders.

# Usage
```
$ make
$ make CFLAGS=-DFREEBSD (When using FreeBSD)
$ ./armlsd 
usage: ./armlsd [-e|-w|-v|-p port] [file]
        -e              erase the entire user code space
        -p port         default: /dev/ttyS0
        -w input.hex    write
        -v input.hex    verify
$ ./armwsd sample.hex (In advance, switch CPU to boot-mode.)
```

# References
1. Analog Devices AN-724: ADuC702x Serial Download Protocol
2. Serial Programming HOWTO [tldp.org]
