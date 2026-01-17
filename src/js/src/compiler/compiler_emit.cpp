/**
 * JavaScript Compiler - bytecode emission helpers
 */

#include "lithium/js/compiler.hpp"

namespace lithium::js {

void Compiler::emit(OpCode op) {
    current_chunk().write(op);
}

void Compiler::emit(u8 byte) {
    current_chunk().write(byte);
}

void Compiler::emit_u16(u16 value) {
    current_chunk().write_u16(value);
}

void Compiler::emit_constant(f64 value) {
    u16 idx = current_chunk().add_constant(value);
    emit(OpCode::LoadConst);
    emit(static_cast<u8>(idx));
}

void Compiler::emit_constant(const String& value) {
    u16 idx = current_chunk().add_constant(value);
    emit(OpCode::LoadConst);
    emit(static_cast<u8>(idx));
}

usize Compiler::emit_jump(OpCode op) {
    emit(op);
    emit(0xFF);
    emit(0xFF);
    return current_chunk().size() - 2;
}

void Compiler::patch_jump(usize offset) {
    current_chunk().patch_jump(offset);
}

void Compiler::emit_loop(usize loop_start) {
    emit(OpCode::Loop);
    usize offset = current_chunk().size() - loop_start + 2;
    emit_u16(static_cast<u16>(offset));
}

u16 Compiler::make_constant(const String& value) {
    return current_chunk().add_constant(value);
}

} // namespace lithium::js
