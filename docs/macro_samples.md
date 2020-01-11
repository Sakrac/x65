# x65macro.i

This is a file under macros and is intended as an example to look at for understanding macro features, it is not super tested for correctness. This information is included in the header file itself but to ease reading copied here. The macros folder also has more detailed documentation.

## Suffix definition

The letters after the period has the following meanings:
- b: byte
- w: word (2 bytes)
- t: triple (3 bytes)
- l: long (4 bytes)
- n: number of bytes in value
- c: copy result to target
- i: immediate, for example add a value to the contents of an address
- x: use the x register for operation as a counter or an offset
- y: use the y register for operation
- r: relative; ry=(zp),y
- a: use the contents of an address for operation (16 bits)
- s: custom step size (instead of +1 or -1) for loops
- p: positive
- m: negative
- o: use label pool for counter

## operations

The base operations provided by these macros are:

- set: Assign a value to the contents of an address
- move: Move the contents of an address to another address
- add: addition
- sub: subtraction
- asrm: arithmetic shift right
- aslm: arithmetic shift left
- neg: negate a number
- abs: make a number positive
- copy: copy memory from one location to another
- for: iterate between two numbers with optional step size
- mnop: insert multiple nop at this point

set.b / .w / .t / .l Value, Target
  - set the contents of an 1-4 byte location to a value
  - uses accumulator

move.b / .w / .t / .l / .n Src,Trg
  - copy 1-4 (or n) bytes from Src location to Trg location
  - uses accumulator

asrm.n Target, Size
  - shift a signed multi byte number right
  - uses accumulator

asrm.nx Target, Size
  - shift a signed multi byte number right offset by the x register
  - no registers touched

aslm.n Target, Size
  - shift a multi byte number left
  - no registers touched

aslm.nx Target, Size
  - shift a multi byte number left offset by the x register
  - no registers changed

neg.cn Source, Target, Size
  - negate and copy a multi byte number
  - uses accumulator

neg.n Target, Size
  - negate a number in place
  - uses accumulator

abs.n Trg, Size
  - make a number absolute
  - uses accumulator

neg.nx Trg, Size
  - negate a number in place offset by the x register
  - uses accumulator

add.n Address1, Address2, Target, Bytes
  - add contents of two memory locations into a target lcoation
  - uses accumulator

sub.n Address1, Address2, Target, Bytes
  - Target = Address1 - Address2
  - uses accumulator

add.ni Address, Value, Target, Bytes
  - add a fixed value to a memory location into a target
  - uses accumulator

sub.ni Address, Value, Target, Bytes
  - Target = Address - Value
  - uses accumulator

add.wi Address, Value, Target
  - Subtract 16 bit Value from contents of Address and store at Target
  - uses accumulator

sub.wi Address1, Address2, Target
  - add contents of two 16 bit addresses into a target 16 bit location
  - uses accumulator

mnop Count
  - add Count nops

copy.x Source, Target, Size
  - copy up to 256 bytes using the x register as a counter
  - uses accumulator and x register

copy.y Source, Target, Size
  - copy up to 256 bytes using the y register as a counter
  - uses accumulator and y register

copy.ry zpSrcPtr,zpTrgPtr,Size
  - copy a fixed length buffer using relative zp y indexing
  - size is up to a page, changing Y and A

copy.ry128 zpSrcPtr,zpTrgPtr,Size
  - copy up to 128 bytes using the y register

copy.o Src,Trg,Size,PoolZP
  - copy more than 256 bytes using zero page label pool addresses
  - uses accumulator, x and y register

copy.a Src,Trg,Size
  - copy more than 256 bytes using absolute indexed in a loop
  - uses accumulator, x and y register

copy.zp Src,Trg,Size,zpTmp1,zpTmp2
  - copy more than 256 bytes using two pairs of zero page values
  - uses accumulator, x and y register

for.x Start, End
  - iterate using the x register from Start to End, End is not inclusive
    so to iterate from 31 to 0 use for.x 31, -1
  - uses x register
  - end for loop with forend macro

for.y Start, End
  - same as for.x but with the y register
  - uses y register
  - end for loop with forend macro

for.w Start, End, Counter
  - for loop for 16 bit counter
  - uses accumulator
  - end for loop with forend macro

for.ws Start, End, Counter, Step
  - for loop for 16 bit counter with a step value
  - uses accumulator
  - end for loop with forend macro

for.wsp Start, End, Counter, Step {
  - for (word Counter=start; Counter<end; Counter += Step), Step>0
  - uses accumulator

for.wsm Start, End, Counter, Step {
  - for (word Counter=start; Counter<end; Counter += Step), Step<0
  - uses accumulator

forend
  - terminates for loops
