#x65 Disassembler

Simple disassembler for 6502, 65C02 and 65816 (default).

## Command Line Options

Typical command line ([*] = optional):

**Usage**

```
x65dsasm code.bin [disassembly.txt] [options]
```

* $(hex start)-$(hex end)
* cpu=[65816/65C02/6502]
* addr=$(hex address to disassemble at)
* mx=[0/1/2/3] (65816 accumulator / index immedate size)
