#! /usr/bin/env ts-node

'use strict'

const enum Section {
  TYPE      = 1,
  IMPORT    = 2,
  FUNC      = 3,
  TABLE     = 4,
  MEMORY    = 5,
  GLOBAL    = 6,
  EXPORT    = 7,
  ELEM      = 9,
  CODE      = 10,
  DATA      = 11,
}

// Wasm opcode
const enum Opcode {
  UNREACHABLE   = 0x00,
  NOP           = 0x01,
  BLOCK         = 0x02,
  LOOP          = 0x03,
  IF            = 0x04,
  ELSE          = 0x05,
  END           = 0x0b,
  BR            = 0x0c,
  BR_IF         = 0x0d,
  BR_TABLE      = 0x0e,
  RETURN        = 0x0f,
  CALL          = 0x10,
  CALL_INDIRECT = 0x11,
  DROP          = 0x1a,
  SELECT        = 0x1b,
  LOCAL_GET     = 0x20,
  LOCAL_SET     = 0x21,
  LOCAL_TEE     = 0x22,
  GLOBAL_GET    = 0x23,
  GLOBAL_SET    = 0x24,
  I32_LOAD      = 0x28,
  I64_LOAD      = 0x29,
  F32_LOAD      = 0x2a,
  F64_LOAD      = 0x2b,
  I32_LOAD8_S   = 0x2c,
  I32_LOAD8_U   = 0x2d,
  I32_LOAD16_S  = 0x2e,
  I32_LOAD16_U  = 0x2f,
  I32_STORE     = 0x36,
  I64_STORE     = 0x37,
  F32_STORE     = 0x38,
  F64_STORE     = 0x39,
  I32_STORE8    = 0x3a,
  I32_STORE16   = 0x3b,
  I64_STORE8    = 0x3c,
  I64_STORE16   = 0x3d,
  I64_STORE32   = 0x3e,
  MEMORY_SIZE   = 0x3f,
  MEMORY_GROW   = 0x40,
  I32_CONST     = 0x41,
  I64_CONST     = 0x42,
  F32_CONST     = 0x43,
  F64_CONST     = 0x44,
  I32_EQZ       = 0x45,
  I32_EQ        = 0x46,
  I32_NE        = 0x47,
  I32_LT_S      = 0x48,
  I32_LT_U      = 0x49,
  I32_GT_S      = 0x4a,
  I32_GT_U      = 0x4b,
  I32_LE_S      = 0x4c,
  I32_LE_U      = 0x4d,
  I32_GE_S      = 0x4e,
  I32_GE_U      = 0x4f,
  I64_EQZ       = 0x50,
  I64_EQ        = 0x51,
  I64_NE        = 0x52,
  I64_LT_S      = 0x53,
  I64_LT_U      = 0x54,
  I64_GT_S      = 0x55,
  I64_GT_U      = 0x56,
  I64_LE_S      = 0x57,
  I64_LE_U      = 0x58,
  I64_GE_S      = 0x59,
  I64_GE_U      = 0x5a,
  F32_EQ        = 0x5b,
  F32_NE        = 0x5c,
  F32_LT        = 0x5d,
  F32_GT        = 0x5e,
  F32_LE        = 0x5f,
  F32_GE        = 0x60,
  F64_EQ        = 0x61,
  F64_NE        = 0x62,
  F64_LT        = 0x63,
  F64_GT        = 0x64,
  F64_LE        = 0x65,
  F64_GE        = 0x66,
  I32_ADD       = 0x6a,
  I32_SUB       = 0x6b,
  I32_MUL       = 0x6c,
  I32_DIV_S     = 0x6d,
  I32_DIV_U     = 0x6e,
  I32_REM_S     = 0x6f,
  I32_REM_U     = 0x70,
  I32_AND       = 0x71,
  I32_OR        = 0x72,
  I32_XOR       = 0x73,
  I32_SHL       = 0x74,
  I32_SHR_S     = 0x75,
  I32_SHR_U     = 0x76,
  I32_ROTL      = 0x77,
  I32_ROTR      = 0x78,
  I64_ADD       = 0x7c,
  I64_SUB       = 0x7d,
  I64_MUL       = 0x7e,
  I64_DIV_S     = 0x7f,
  I64_DIV_U     = 0x80,
  I64_REM_S     = 0x81,
  I64_REM_U     = 0x82,
  I64_AND       = 0x83,
  I64_OR        = 0x84,
  I64_XOR       = 0x85,
  I64_SHL       = 0x86,
  I64_SHR_S     = 0x87,
  I64_SHR_U     = 0x88,
  I64_ROTL      = 0x89,
  I64_ROTR      = 0x8a,
  F32_ABS       = 0x8b,
  F32_NEG       = 0x8c,
  F32_ADD       = 0x92,
  F32_SUB       = 0x93,
  F32_MUL       = 0x94,
  F32_DIV       = 0x95,
  F64_ABS       = 0x99,
  F64_NEG       = 0x9a,
  F64_ADD       = 0xa0,
  F64_SUB       = 0xa1,
  F64_MUL       = 0xa2,
  F64_DIV       = 0xa3,
  I32_WRAP_I64        = 0xa7,  // i32 <- i64
  I32_TRUNC_F32_S     = 0xa8,  // i32 <- f32
  I32_TRUNC_F32_U     = 0xa9,  // i32 <- f32
  I32_TRUNC_F64_S     = 0xaa,  // i32 <- f64
  I32_TRUNC_F64_U     = 0xab,  // i32 <- f64
  I64_EXTEND_I32_S    = 0xac,  // i64 <- i32
  I64_EXTEND_I32_U    = 0xad,  // i64 <- i32
  I64_TRUNC_F32_S     = 0xae,  // i64 <- f32
  I64_TRUNC_F32_U     = 0xaf,  // i64 <- f32
  I64_TRUNC_F64_S     = 0xb0,  // i64 <- f64
  I64_TRUNC_F64_U     = 0xb1,  // i64 <- f64
  F32_CONVERT_I32_S   = 0xb2,  // f32 <- i32
  F32_CONVERT_I32_U   = 0xb3,  // f32 <- i32
  F32_CONVERT_I64_S   = 0xb4,  // f32 <- i64
  F32_CONVERT_I64_U   = 0xb5,  // f32 <- i64
  F32_DEMOTE_F64      = 0xb6,  // f32 <- f64
  F64_CONVERT_I32_S   = 0xb7,  // f64 <- i32
  F64_CONVERT_I32_U   = 0xb8,  // f64 <- i32
  F64_CONVERT_I64_S   = 0xb9,  // f64 <- i64
  F64_CONVERT_I64_U   = 0xba,  // f64 <- i64
  F64_PROMOTE_F32     = 0xbb,  // f64 <- f32
  I32_REINTERPRET_F32 = 0xbc,  // i32 <- f32
  I64_REINTERPRET_F64 = 0xbd,  // i64 <- f64
}

const enum WasmType {
  VOID       = 0x40,
  FUNC       = 0x60,
  F64        = 0x7c,
  F32        = 0x7d,
  I64        = 0x7e,
  I32        = 0x7f,
}

const enum ImportKind {
  FUNC = 0,
  MEMORY = 2,
  GLOBAL = 3,
}

const enum OpKind {
  MISC,
  BLOCK,
  LOAD,
  STORE,
  BR_TABLE,
}

const InstTable = new Map([
  [Opcode.UNREACHABLE, {op: 'unreachable'}],
  [Opcode.NOP, {op: 'nop'}],
  [Opcode.BLOCK, {op: 'block', operands: ['type'], opKind: OpKind.BLOCK}],
  [Opcode.LOOP, {op: 'loop', operands: ['type'], opKind: OpKind.BLOCK}],
  [Opcode.IF, {op: 'if', operands: ['type'], opKind: OpKind.BLOCK}],
  [Opcode.ELSE, {op: 'else'}],
  [Opcode.END, {op: 'end'}],
  [Opcode.BR, {op: 'br', operands: ['uleb128']}],
  [Opcode.BR_IF, {op: 'br_if', operands: ['uleb128']}],
  [Opcode.BR_TABLE, {op: 'br_table', operands: ['uleb128array', 'uleb128'], opKind: OpKind.BR_TABLE}],
  [Opcode.RETURN, {op: 'return'}],
  [Opcode.CALL, {op: 'call', operands: ['uleb128']}],
  [Opcode.CALL_INDIRECT, {op: 'call_indirect', operands: ['uleb128', 'uleb128']}],
  [Opcode.DROP, {op: 'drop'}],
  [Opcode.SELECT, {op: 'select'}],
  [Opcode.LOCAL_GET, {op: 'local.get', operands: ['uleb128']}],
  [Opcode.LOCAL_SET, {op: 'local.set', operands: ['uleb128']}],
  [Opcode.LOCAL_TEE, {op: 'local.tee', operands: ['uleb128']}],
  [Opcode.GLOBAL_GET, {op: 'global.get', operands: ['uleb128']}],
  [Opcode.GLOBAL_SET, {op: 'global.set', operands: ['uleb128']}],
  [Opcode.I32_LOAD, {op: 'i32.load', operands: ['uleb128', 'uleb128'], opKind: OpKind.LOAD}],
  [Opcode.I64_LOAD, {op: 'i64.load', operands: ['uleb128', 'uleb128'], opKind: OpKind.LOAD}],
  [Opcode.F32_LOAD, {op: 'f32.load', operands: ['uleb128', 'uleb128'], opKind: OpKind.LOAD}],
  [Opcode.F64_LOAD, {op: 'f64.load', operands: ['uleb128', 'uleb128'], opKind: OpKind.LOAD}],
  [Opcode.I32_STORE, {op: 'i32.store', operands: ['uleb128', 'uleb128'], opKind: OpKind.STORE}],
  [Opcode.I64_STORE, {op: 'i64.store', operands: ['uleb128', 'uleb128'], opKind: OpKind.STORE}],
  [Opcode.F32_STORE, {op: 'f32.store', operands: ['uleb128', 'uleb128'], opKind: OpKind.STORE}],
  [Opcode.F64_STORE, {op: 'f64.store', operands: ['uleb128', 'uleb128'], opKind: OpKind.STORE}],
  [Opcode.I32_LOAD8_S, {op: 'i32.load8_s', operands: ['uleb128', 'uleb128'], opKind: OpKind.LOAD}],
  [Opcode.I32_LOAD8_U, {op: 'i32.load8_u', operands: ['uleb128', 'uleb128'], opKind: OpKind.LOAD}],
  [Opcode.I32_LOAD16_S, {op: 'i32.load16_s', operands: ['uleb128', 'uleb128'], opKind: OpKind.LOAD}],
  [Opcode.I32_LOAD16_U, {op: 'i32.load16_u', operands: ['uleb128', 'uleb128'], opKind: OpKind.LOAD}],
  [Opcode.I32_STORE8, {op: 'i32.store8', operands: ['uleb128', 'uleb128'], opKind: OpKind.STORE}],
  [Opcode.I32_STORE16, {op: 'i32.store16', operands: ['uleb128', 'uleb128'], opKind: OpKind.STORE}],
  [Opcode.I64_STORE8, {op: 'i64.store8', operands: ['uleb128', 'uleb128'], opKind: OpKind.STORE}],
  [Opcode.I64_STORE16, {op: 'i64.store16', operands: ['uleb128', 'uleb128'], opKind: OpKind.STORE}],
  [Opcode.I64_STORE32, {op: 'i64.store32', operands: ['uleb128', 'uleb128'], opKind: OpKind.STORE}],
  [Opcode.MEMORY_SIZE, {op: 'memory.size', operands: ['uleb128']}],
  [Opcode.MEMORY_GROW, {op: 'memory.grow', operands: ['uleb128']}],
  [Opcode.I32_CONST, {op: 'i32.const', operands: ['leb128']}],
  [Opcode.I64_CONST, {op: 'i64.const', operands: ['leb128']}],
  [Opcode.F32_CONST, {op: 'f32.const', operands: ['f32']}],
  [Opcode.F64_CONST, {op: 'f64.const', operands: ['f64']}],
  [Opcode.I32_EQZ, {op: 'i32.eqz'}],
  [Opcode.I32_EQ, {op: 'i32.eq'}],
  [Opcode.I32_NE, {op: 'i32.ne'}],
  [Opcode.I32_LT_S, {op: 'i32.lt_s'}],
  [Opcode.I32_LT_U, {op: 'i32.lt_u'}],
  [Opcode.I32_GT_S, {op: 'i32.gt_s'}],
  [Opcode.I32_GT_U, {op: 'i32.gt_u'}],
  [Opcode.I32_LE_S, {op: 'i32.le_s'}],
  [Opcode.I32_LE_U, {op: 'i32.le_u'}],
  [Opcode.I32_GE_S, {op: 'i32.ge_s'}],
  [Opcode.I32_GE_U, {op: 'i32.ge_u'}],
  [Opcode.I64_EQZ, {op: 'i64.eqz'}],
  [Opcode.I64_EQ, {op: 'i64.eq'}],
  [Opcode.I64_NE, {op: 'i64.ne'}],
  [Opcode.I64_LT_S, {op: 'i64.lt_s'}],
  [Opcode.I64_LT_U, {op: 'i64.lt_u'}],
  [Opcode.I64_GT_S, {op: 'i64.gt_s'}],
  [Opcode.I64_GT_U, {op: 'i64.gt_u'}],
  [Opcode.I64_LE_S, {op: 'i64.le_s'}],
  [Opcode.I64_LE_U, {op: 'i64.le_u'}],
  [Opcode.I64_GE_S, {op: 'i64.ge_s'}],
  [Opcode.I64_GE_U, {op: 'i64.ge_u'}],
  [Opcode.F32_EQ, {op: 'f32.eq'}],
  [Opcode.F32_NE, {op: 'f32.ne'}],
  [Opcode.F32_LT, {op: 'f32.lt'}],
  [Opcode.F32_GT, {op: 'f32.gt'}],
  [Opcode.F32_LE, {op: 'f32.le'}],
  [Opcode.F32_GE, {op: 'f32.ge'}],
  [Opcode.F64_EQ, {op: 'f64.eq'}],
  [Opcode.F64_NE, {op: 'f64.ne'}],
  [Opcode.F64_LT, {op: 'f64.lt'}],
  [Opcode.F64_GT, {op: 'f64.gt'}],
  [Opcode.F64_LE, {op: 'f64.le'}],
  [Opcode.F64_GE, {op: 'f64.ge'}],
  [Opcode.I32_ADD, {op: 'i32.add'}],
  [Opcode.I32_SUB, {op: 'i32.sub'}],
  [Opcode.I32_MUL, {op: 'i32.mul'}],
  [Opcode.I32_DIV_S, {op: 'i32.div_s'}],
  [Opcode.I32_DIV_U, {op: 'i32.div_u'}],
  [Opcode.I32_REM_S, {op: 'i32.rem_s'}],
  [Opcode.I32_REM_U, {op: 'i32.rem_u'}],
  [Opcode.I32_AND, {op: 'i32.and'}],
  [Opcode.I32_OR, {op: 'i32.or'}],
  [Opcode.I32_XOR, {op: 'i32.xor'}],
  [Opcode.I32_SHL, {op: 'i32.shl'}],
  [Opcode.I32_SHR_S, {op: 'i32.shr_s'}],
  [Opcode.I32_SHR_U, {op: 'i32.shr_u'}],
  [Opcode.I32_ROTL, {op: 'i32.rotl'}],
  [Opcode.I32_ROTR, {op: 'i32.rotr'}],
  [Opcode.I64_ADD, {op: 'i64.add'}],
  [Opcode.I64_SUB, {op: 'i64.sub'}],
  [Opcode.I64_MUL, {op: 'i64.mul'}],
  [Opcode.I64_DIV_S, {op: 'i64.div_s'}],
  [Opcode.I64_DIV_U, {op: 'i64.div_u'}],
  [Opcode.I64_REM_S, {op: 'i64.rem_s'}],
  [Opcode.I64_REM_U, {op: 'i64.rem_u'}],
  [Opcode.I64_AND, {op: 'i64.and'}],
  [Opcode.I64_OR, {op: 'i64.or'}],
  [Opcode.I64_XOR, {op: 'i64.xor'}],
  [Opcode.I64_SHL, {op: 'i64.shl'}],
  [Opcode.I64_SHR_S, {op: 'i64.shr_s'}],
  [Opcode.I64_SHR_U, {op: 'i64.shr_u'}],
  [Opcode.I64_ROTL, {op: 'i64.rotl'}],
  [Opcode.I64_ROTR, {op: 'i64.rotr'}],
  [Opcode.F32_ABS, {op: 'f32.abs'}],
  [Opcode.F32_NEG, {op: 'f32.neg'}],
  [Opcode.F32_ADD, {op: 'f32.add'}],
  [Opcode.F32_SUB, {op: 'f32.sub'}],
  [Opcode.F32_MUL, {op: 'f32.mul'}],
  [Opcode.F32_DIV, {op: 'f32.div'}],
  [Opcode.F64_ABS, {op: 'f64.abs'}],
  [Opcode.F64_NEG, {op: 'f64.neg'}],
  [Opcode.F64_ADD, {op: 'f64.add'}],
  [Opcode.F64_SUB, {op: 'f64.sub'}],
  [Opcode.F64_MUL, {op: 'f64.mul'}],
  [Opcode.F64_DIV, {op: 'f64.div'}],

  [Opcode.I32_WRAP_I64, {op: 'i32.wrap_i64'}],
  [Opcode.I32_TRUNC_F32_S, {op: 'i32.trunc_f32_s'}],
  [Opcode.I32_TRUNC_F32_U, {op: 'i32.trunc_f32_u'}],
  [Opcode.I32_TRUNC_F64_S, {op: 'i32.trunc_f64_s'}],
  [Opcode.I32_TRUNC_F64_U, {op: 'i32.trunc_f64_u'}],
  [Opcode.I64_EXTEND_I32_S, {op: 'i64.extend_i32_s'}],
  [Opcode.I64_EXTEND_I32_U, {op: 'i64.extend_i32_u'}],
  [Opcode.I64_TRUNC_F32_S, {op: 'i64.trunc_f32_s'}],
  [Opcode.I64_TRUNC_F32_U, {op: 'i64.trunc_f32_u'}],
  [Opcode.I64_TRUNC_F64_S, {op: 'i64.trunc_f64_s'}],
  [Opcode.I64_TRUNC_F64_U, {op: 'i64.trunc_f64_u'}],
  [Opcode.F32_CONVERT_I32_S, {op: 'f32.convert_i32_s'}],
  [Opcode.F32_CONVERT_I32_U, {op: 'f32.convert_i32_u'}],
  [Opcode.F32_DEMOTE_F64, {op: 'f32.demote_f64'}],
  [Opcode.F32_CONVERT_I64_S, {op: 'f32.convert_i64_s'}],
  [Opcode.F32_CONVERT_I64_U, {op: 'f32.convert_i64_u'}],
  [Opcode.F64_CONVERT_I32_S, {op: 'f64.convert_i32_s'}],
  [Opcode.F64_CONVERT_I32_U, {op: 'f64.convert_i32_u'}],
  [Opcode.F64_CONVERT_I64_S, {op: 'f64.convert_i64_s'}],
  [Opcode.F64_CONVERT_I64_U, {op: 'f64.convert_i64_u'}],
  [Opcode.F64_PROMOTE_F32, {op: 'f64.promote_f32'}],
  [Opcode.I32_REINTERPRET_F32, {op: 'i32.reinterpret_f32'}],
  [Opcode.I64_REINTERPRET_F64, {op: 'i64.reinterpret_f64'}],
])

class BufferReader {
  private offset = 0
  private byteArray: Uint8Array

  constructor(buffer: ArrayBuffer) {
    this.byteArray = new Uint8Array(buffer)
  }

  public getOffset(): number { return this.offset }
  public setOffset(offset: number): void { this.offset = offset }

  public isEof(): boolean { return this.offset >= this.byteArray.byteLength }

  public readu8(): number {
    return this.byteArray[this.offset++]
  }

  public readi32(): number {
    const value = new Int32Array(this.byteArray.buffer, this.offset, 1)[0]
    this.offset += 4
    return value
  }

  public readf32(): number {
    let buffer = this.byteArray.buffer
    let offset = this.offset
    if ((offset & 3) !== 0) {
      buffer = this.byteArray.slice(offset, offset + 4).buffer
      offset = 0
    }
    const value = new Float32Array(buffer, offset, 1)[0]
    this.offset += 4
    return value
  }

  public readf64(): number {
    let buffer = this.byteArray.buffer
    let offset = this.offset
    if ((offset & 7) !== 0) {
      buffer = this.byteArray.slice(offset, offset + 8).buffer
      offset = 0
    }
    const value = new Float64Array(buffer, offset, 1)[0]
    this.offset += 8
    return value
  }

  public readLeb128(): number {
    let x = 0
    let bits = 0
    let ofs = this.offset
    while (ofs < this.byteArray.byteLength) {
      const c = this.byteArray[ofs++]
      x |= (c & 0x7f) << bits
      bits += 7
      if ((c & 0x80) === 0) {
        if ((c & 0x40) !== 0)
          x -= 1 << bits
        break
      }
    }
    this.offset = ofs
    return x
  }

  public readUleb128(): number {
    let x = 0
    let bits = 0
    let ofs = this.offset
    while (ofs < this.byteArray.byteLength) {
      const c = this.byteArray[ofs++]
      x |= (c & 0x7f) << bits
      bits += 7
      if ((c & 0x80) === 0)
        break
    }
    this.offset = ofs
    return x
  }

  public readString(): string {
    const len = this.readUleb128()
    const u8array = this.byteArray.slice(this.offset, this.offset + len)
    this.offset += len
    return new TextDecoder('utf-8').decode(u8array)
  }

  public u8array(length: number): Uint8Array {
    const u8array = this.byteArray.slice(this.offset, length)
    this.offset += length
    return u8array
  }
}

type FuncTypeInfo = {type: string, params: Array<Type>, results: Array<Type>}
class Type {
  private type: number | FuncTypeInfo

  constructor(type: number | FuncTypeInfo) {
    this.type = type
  }

  public getType(): number | FuncTypeInfo { return this.type }

  public toString(): string {
    if (typeof(this.type) === 'object') {
      const params = this.type.params.length === 0 ? '' : ` (param ${this.type.params.map(param => `${param}`).join(' ')})`
      const results = this.type.results.length === 0 ? '' : ` (result ${this.type.results.map(param => `${param}`).join(' ')})`
      return `(func${params}${results})`
    } else {
      switch (this.type) {
      case WasmType.VOID:  return 'void'
      case WasmType.I32:   return 'i32'
      case WasmType.I64:   return 'i64'
      case WasmType.F32:   return 'f32'
      case WasmType.F64:   return 'f64'
      default:  throw `Unhandled: ${this.type}`
      }
    }
  }
}

function readType(bufferReader: BufferReader): Type {
  const t = bufferReader.readu8()
  switch (t) {
  case WasmType.VOID:
  case WasmType.I32:
  case WasmType.I64:
  case WasmType.F32:
  case WasmType.F64:
    return new Type(t)
  case WasmType.FUNC:
    {
      const numParams = bufferReader.readUleb128()
      const params = [...Array(numParams)].map(() => readType(bufferReader))
      const num_results = bufferReader.readUleb128()
      const results = [...Array(num_results)].map(() => readType(bufferReader))
      return new Type({type: 'func', params: params, results: results})
    }
  default:
    throw `Unhnadled type: at 0x${(bufferReader.getOffset() - 1).toString(16)}`
  }
}

function readGlobalValue(bufferReader: BufferReader): any {
  const op = bufferReader.readu8()
  switch (op) {
  case Opcode.I32_CONST:
  case Opcode.I64_CONST:
    return bufferReader.readLeb128()
  case Opcode.F32_CONST:
    return bufferReader.readf32()
  case Opcode.F64_CONST:
    return bufferReader.readf64()
  default:
    throw `Unhnadled type: ${op} at ${(bufferReader.getOffset() - 1).toString(16)}`
  }
}

type Operand = number | Array<number>

function readOperand(bufferReader: BufferReader, operand: string): Operand|Type {
  switch (operand) {
  case 'type':
    return readType(bufferReader)
  case 'leb128':
    return bufferReader.readLeb128()
  case 'uleb128':
    return bufferReader.readUleb128()
  case 'uleb128array':
    {
      const count = bufferReader.readUleb128()
      return [...Array(count)].map(_ => bufferReader.readUleb128())
    }
  case 'f32':
    return bufferReader.readf32()
  case 'f64':
    return bufferReader.readf64()
  default:
    throw `Unhandled operand: ${operand} at 0x${bufferReader.getOffset().toString(16)}`
  }
}

class Inst {
  public opcode: Opcode
  public opKind: OpKind
  public opstr: string
  public operands?: Array<Operand|Type>
  public operandTypes?: Array<string>
}

function readInst(bufferReader: BufferReader): Inst {
  const op = bufferReader.readu8()

  const table = InstTable.get(op)
  if (table == null) {
    throw `Unhandled op: 0x${op.toString(16).padStart(2, '0')} at 0x${bufferReader.getOffset().toString(16)}`
  }

  const inst: Inst = {opcode: op as Opcode, opKind: table.opKind || OpKind.MISC, opstr: table.op}
  if (table.operands != null) {
    inst.operandTypes = table.operands
    inst.operands = table.operands.map(operand => readOperand(bufferReader, operand))
  }
  return inst
}

let SPACES = '    '
function makeIndent(indent: number): string {
  const len = indent * 2
  while (len > SPACES.length)
    SPACES += SPACES
  return SPACES.slice(0, len)
}

function addr(adr: number): string {
  return `(;${adr.toString(16).padStart(5, '0')};)`
}

export class DisWasm {
  private bufferReader: BufferReader
  private version = -1
  private types = new Array<Type>()
  private functions = new Array<number>()
  private codes = new Array<Array<Inst>>()
  private importFuncCount = 0
  private log: (s: string)=>void = console.log

  constructor(buffer: ArrayBuffer) {
    this.bufferReader = new BufferReader(buffer)
  }

  public setLogFunc(logFunc: (s: string)=>void): void {
    this.log = logFunc
  }

  public dump(): void {
    if (!this.checkHeader())
      throw Error('No wasm header')
    this.log('(module')
    this.log(`;; WASM version: ${this.version}`)
    this.loadSections()
    this.log(')')
  }

  private checkHeader(): boolean {
    const magic = this.bufferReader.u8array(4)
    if (new TextDecoder('utf-8').decode(magic) !== '\x00asm')
      return false
    this.version = this.bufferReader.readi32()
    return true
  }

  private loadSections(): void {
    const SectionNames = [
      null,
      'TYPE',
      'IMPORT',
      'FUNC',
      'TABLE',
      'MEMORY',
      'GLOBAL',
      'EXPORT',
      null,
      'ELEM',
      'CODE',
      'DATA',
    ]

    while (!this.bufferReader.isEof()) {
      const offset = this.bufferReader.getOffset()
      const sec = this.bufferReader.readu8() as Section
      const len = this.bufferReader.readUleb128()
      const section_start_offset = this.bufferReader.getOffset()

      this.log(`\n;;=== 0x${offset.toString(16)}: ${SectionNames[sec] || `(section ${sec})`}, len=${len}`)
      switch (sec) {
      case Section.TABLE:
      case Section.ELEM:
        // TODO
        break

      case Section.TYPE:
        this.readTypeSection()
        break

      case Section.IMPORT:
        this.readImportSection()
        break

      case Section.FUNC:
        this.readFuncSection()
        break

      case Section.MEMORY:
        this.readMemorySection()
        break

      case Section.GLOBAL:
        this.readGlobalSection()
        break

      case Section.EXPORT:
        this.readExportSection()
        break

      case Section.CODE:
        this.readCodeSection()
        break

      case Section.DATA:
        this.readDataSection()
        break

      default:
        throw `Unhandled section: ${sec}, offset=0x${offset.toString(16)}, len=${len}`
      }

      this.bufferReader.setOffset(section_start_offset + len)
    }
  }

  private readTypeSection(): void {
    const num = this.bufferReader.readUleb128()
    for (let i = 0; i < num; ++i) {
      const offset = this.bufferReader.getOffset()
      const type = readType(this.bufferReader)
      this.types.push(type)
      this.log(`${addr(offset)} (type (;${i};) ${type.toString()})`)
    }
  }

  private readImportSection(): void {
    const num = this.bufferReader.readUleb128()
    for (let i = 0; i < num; ++i) {
      const offset = this.bufferReader.getOffset()
      const modName = this.bufferReader.readString()
      const name = this.bufferReader.readString()
      const kind = this.bufferReader.readu8()
      if (kind !== ImportKind.FUNC)
        throw(`Illegal import kind: ${kind}`)
      const index = this.bufferReader.readUleb128()
      this.log(`${addr(offset)} (import "${modName}" "${name}" (func (;${this.importFuncCount};) (type ${index})))`)
      this.importFuncCount += 1
    }
  }

  private readFuncSection(): void {
    const num = this.bufferReader.readUleb128()
    for (let i = 0; i < num; ++i) {
      const typeIndex = this.bufferReader.readUleb128()
      this.functions.push(typeIndex)
    }
  }

  private readMemorySection(): void {
    const num = this.bufferReader.readUleb128()
    for (let i = 0; i < num; ++i) {
      const offset = this.bufferReader.getOffset()
      const index = this.bufferReader.readUleb128()
      const pageCount = this.bufferReader.readUleb128()
      this.log(`${addr(offset)} (memory (;${index};) ${pageCount})`)
    }
  }

  private readGlobalSection(): void {
    const num = this.bufferReader.readUleb128()
    for (let i = 0; i < num; ++i) {
      const offset = this.bufferReader.getOffset()
      const type = readType(this.bufferReader)
      const mut = this.bufferReader.readu8()
      const value = readGlobalValue(this.bufferReader)
      this.log(`${addr(offset)} (global (;${i};) ${mut !== 0 ? `(mut ${type})` : `${type}`} (${type}.const ${value}))`)
      this.bufferReader.readu8()  // Skip OP_END
    }
  }

  private readExportSection(): void {
    const KindNames = ['func', null, 'memory', 'global']

    const num = this.bufferReader.readUleb128()
    for (let i = 0; i < num; ++i) {
      const offset = this.bufferReader.getOffset()
      const name = this.bufferReader.readString()
      const kind = this.bufferReader.readu8()
      const index = this.bufferReader.readUleb128()
      this.log(`${addr(offset)} (export "${name}" (${KindNames[kind] || `kind=${kind}`} ${index}))`)
    }
  }

  private readCodeSection(): void {
    const num = this.bufferReader.readUleb128()
    for (let i = 0; i < num; ++i) {
      const offset = this.bufferReader.getOffset()
      const typeIndex = this.functions[i]
      this.log(`${addr(offset)} (func (;${i + this.importFuncCount};) (type ${typeIndex})`)
      const code = this.readCode()
      this.codes.push(code)
    }
  }

  private readCode(): Inst[] {
    const bodySize = this.bufferReader.readUleb128()
    const endOfs = this.bufferReader.getOffset() + bodySize
    const localDeclCount = this.bufferReader.readUleb128()
    if (localDeclCount > 0) {
      for (let i = 0; i < localDeclCount; ++i) {
        const offset = this.bufferReader.getOffset()
        const num = this.bufferReader.readUleb128()
        const t = readType(this.bufferReader)
        this.log(`${addr(offset)}   ${[...Array(num)].map(_ => `(local ${t})`).join(' ')}`)
      }
    }

    const code = new Array<Inst>()
    let indent = 1
    while (this.bufferReader.getOffset() < endOfs) {
      const offset = this.bufferReader.getOffset()
      const inst = readInst(this.bufferReader)
      code.push(inst)

      switch (inst.opcode) {
      case Opcode.ELSE: case Opcode.END:
        --indent
        if (indent === 0 && inst.opcode === Opcode.END) {
          this.log(`${addr(offset)} )`)
          continue
        }
        break
      }

      const spaces = makeIndent(indent)
      let operands = ''
      if (inst.operands != null) {
        switch (inst.opKind) {
        case OpKind.BLOCK:
          {
            const t = inst.operands[0] as Type
            if (t.getType() !== WasmType.VOID)
              operands = `(result ${t.toString()})`
          }
          break
        case OpKind.LOAD:
        case OpKind.STORE:
          break
        case OpKind.BR_TABLE:
          operands = `${(inst.operands[0] as Array<number>).join(' ')} ${inst.operands[1]}`
          break
        default:
          operands = inst.operands.map((x) => x.toString()).join(' ')
          break
        }
      }
      this.log(`${addr(offset)} ${spaces}${inst.opstr} ${operands}`.trimEnd())

      switch (inst.opcode) {
      case Opcode.IF: case Opcode.BLOCK: case Opcode.LOOP: case Opcode.ELSE:
        ++indent
        break
      }
    }
    return code
  }

  private readDataSection(): void {
    const num = this.bufferReader.readUleb128()
    for (let i = 0; i < num; ++i) {
      const offset = this.bufferReader.getOffset()
      /*const flag =*/ this.bufferReader.readUleb128()
      if (this.bufferReader.readu8() !== Opcode.I32_CONST ||
          this.bufferReader.readu8() !== 0x00 ||
          this.bufferReader.readu8() !== Opcode.END)
        throw 'Illegal data'
      const datasize = this.bufferReader.readUleb128()
      const data = new Array<string>(datasize)
      for (let i = 0; i < datasize; ++i) {
        const c = this.bufferReader.readu8()
        data[i] = (0x20 <= c && c <= 0x7e) ? String.fromCharCode(c) : `\\${c.toString(16).padStart(2, '0')}`
      }

      this.log(`${addr(offset)} (data (;${i};) (i32.const 0) "${data.join('')}")`)
    }
  }
}
