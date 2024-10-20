#ifndef _CPU_HEAD_H_
#define _CPU_HEAD_H_

#define READ_HANDLER(name)  static UINT8 name(UINT32 address, struct MemoryReadByte *psMemRead)
#define WRITE_HANDLER(name)  static void name(UINT32 address, UINT8 data, struct MemoryWriteByte *psMemWrite)

#define WRITE8_HANDLER(name)  static void name(UINT32 address, UINT8 data, struct MemoryWriteByte *psMemWrite)
#define WRITE16_HANDLER(name)  static void name(UINT32 address, UINT8 data, struct MemoryWriteByte *psMemWrite)

#define MEM_WRITE(name) struct MemoryWriteByte name[] = {
#define MEM_READ(name)  struct MemoryReadByte name[] = {
#define MEM_ADDR(start,end,routine) {start,end,routine},

#define MEM_END {(UINT32) -1,(UINT32) -1,NULL}};

#define PORT_WRITE_HANDLER(name) static void name(UINT16 port, UINT8 data, struct z80PortWrite *pPW)
#define PORT_READ_HANDLER(name) static UINT16 name(UINT16 port, struct z80PortRead *pPR)
#define PORT_WRITE(name) struct z80PortWrite name[] = {
#define PORT_READ(name) struct z80PortRead name[] = {
#define PORT_ADDR(start,end,routine) {start,end,routine},
#define PORT_END {(UINT16) -1, (UINT16) -1, NULL}};

#endif 