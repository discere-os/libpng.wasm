/**
 * Type definitions for Libpng WASM
 */

export interface LIBPNGModule {
  _malloc: (size: number) => number
  _free: (ptr: number) => void
  HEAPU8: Uint8Array
  setValue: (ptr: number, value: number, type: string) => void
  getValue: (ptr: number, type: string) => number
}

export class LIBPNGError extends Error {
  constructor(message: string) {
    super(message)
    this.name = 'LIBPNGError'
  }
}
