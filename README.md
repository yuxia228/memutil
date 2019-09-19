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

### example

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

### example

```bash
# [0x12345678] 0x7856eeee
echo "write 4 0x12345678 0xAAAAAAAA" > /dev/memd
# [0x12345678] 0xAAAAAAAA
```

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

