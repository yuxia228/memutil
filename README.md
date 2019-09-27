# memutil

## Description

This driver reads and writes memory on the linux machine.

## Build

```bash
make
```

### Using Specific kernel source or Cross Compile

```bash
cp sample.config .config
vim .config
# edit .config file
make
```

## Usage

### Read

```bash
echo "read|r length addr [read_cycle]" > /dev/memd
```

- length: read size[byte]
- addr: read top address(hex)
- read_cycle(option): read cycle[byte] (default is 4)
  - list: [ 1, 2, 4];

#### example

```bash
echo "read 8 0x12345678 4" > /dev/memd
# [0x12345678] 0x78563412
echo "read 10 0x12345678 2" > /dev/memd
# [0x12345678] 0x3412
echo "read 8 0x12345678 1" > /dev/memd
# [0x12345678] 0x12
```

### Write

```bash
echo "write|r length addr value" > /dev/memd
```

- length: write size[byte]
  - list[1, 2, 4]
- addr: write top address(hex)
- value: write data(hex)

#### example

```bash
# [0x12345678] 0x7856eeee
echo "write 4 0x12345678 0xAAAAAAAA" > /dev/memd
# [0x12345678] 0xAAAAAAAA
```

### Monitor

```bash
echo "monitor|m length addr read_cycle" > /dev/memd
cat /dev/memd
```

- length: read size[byte]
  - Max: 1024
- addr: read top address(hex)
- read_cycle(option): read cycle[byte] (default is 4)
  - list: [ 1, 2, 4];

#### example

```bash
echo "monitor|m length addr value" > /dev/memd
cat /dev/memd
# when value is changed, value is shown
```

### Polling

```bash
echo "polling|p length addr read_cycle interval" > /dev/memd
cat /dev/memd
```

- length: read size[byte]
  - Max: 1024
- addr: read top address(hex)
- read_cycle(option): read cycle[byte] (default is 4)
  - list: [ 1, 2, 4];
- interval: interval time[msec]

#### example

```bash
echo "polling 32 0x12345678 1 1000" > /dev/memd
cat /dev/memd
#                0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
#[0x12345678] 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 
#[0x12345688] 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
.
.
.
```

### wrapper script

- memutil.sh

#### Usage 

- See ```./memutil.sh -h```


### bug?

iowrite8 and iowrite16 is strange...

```bash
# before: [0x12345678] 0x78563412
echo "write 1 0x12345678 0xFF" > /dev/memd
# result:   [0x12345678] 0xFFFFFFFF
# expected: [0x12345678] 0x785634FF

# [0x12345678] 0x785634ff
echo "write 2 0x12345678 0xDDEE" > /dev/memd
# result:   [0x12345678] 0xDDEEDDEE
# expected: [0x12345678] 0x7856DDEE
```

