/**
 * Bytecode Chunk implementation
 */

#include "lithium/js/bytecode.hpp"

namespace lithium::js {

void Chunk::write(OpCode op) {
    m_code.push_back(static_cast<u8>(op));
}

void Chunk::write_u8(u8 byte) {
    m_code.push_back(byte);
}

void Chunk::write_u16(u16 value) {
    m_code.push_back(static_cast<u8>((value >> 8) & 0xFF));
    m_code.push_back(static_cast<u8>(value & 0xFF));
}

void Chunk::write_i16(i16 value) {
    write_u16(static_cast<u16>(value));
}

u16 Chunk::read_u16(usize offset) const {
    return static_cast<u16>((m_code[offset] << 8) | m_code[offset + 1]);
}

i16 Chunk::read_i16(usize offset) const {
    return static_cast<i16>(read_u16(offset));
}

u16 Chunk::add_constant(const Value& value) {
    m_constants.push_back(value);
    return static_cast<u16>(m_constants.size() - 1);
}

void Chunk::patch_i16(usize operand_offset, i16 value) {
    m_code[operand_offset] = static_cast<u8>((value >> 8) & 0xFF);
    m_code[operand_offset + 1] = static_cast<u8>(value & 0xFF);
}

} // namespace lithium::js
