/**
 * Bytecode Chunk implementation
 */

#include "lithium/js/bytecode.hpp"
#include <sstream>
#include <iomanip>

namespace lithium::js {

// ============================================================================
// Chunk implementation
// ============================================================================

void Chunk::write(OpCode op) {
    m_code.push_back(static_cast<u8>(op));
}

void Chunk::write(u8 byte) {
    m_code.push_back(byte);
}

void Chunk::write_u16(u16 value) {
    m_code.push_back(static_cast<u8>((value >> 8) & 0xFF));
    m_code.push_back(static_cast<u8>(value & 0xFF));
}

void Chunk::write_i16(i16 value) {
    write_u16(static_cast<u16>(value));
}

void Chunk::patch_jump(usize offset) {
    // Calculate jump distance from after the jump instruction
    i16 jump = static_cast<i16>(m_code.size() - offset - 2);
    m_code[offset] = static_cast<u8>((jump >> 8) & 0xFF);
    m_code[offset + 1] = static_cast<u8>(jump & 0xFF);
}

void Chunk::patch_jump(usize offset, i16 target) {
    m_code[offset] = static_cast<u8>((target >> 8) & 0xFF);
    m_code[offset + 1] = static_cast<u8>(target & 0xFF);
}

u16 Chunk::add_constant(f64 value) {
    // Check if constant already exists
    for (usize i = 0; i < m_constants.size(); ++i) {
        if (auto* d = std::get_if<f64>(&m_constants[i])) {
            if (*d == value) {
                return static_cast<u16>(i);
            }
        }
    }
    m_constants.push_back(value);
    return static_cast<u16>(m_constants.size() - 1);
}

u16 Chunk::add_constant(const String& value) {
    // Check if constant already exists
    for (usize i = 0; i < m_constants.size(); ++i) {
        if (auto* s = std::get_if<String>(&m_constants[i])) {
            if (*s == value) {
                return static_cast<u16>(i);
            }
        }
    }
    m_constants.push_back(value);
    return static_cast<u16>(m_constants.size() - 1);
}

u16 Chunk::add_constant(bool value) {
    // Check if constant already exists
    for (usize i = 0; i < m_constants.size(); ++i) {
        if (auto* b = std::get_if<bool>(&m_constants[i])) {
            if (*b == value) {
                return static_cast<u16>(i);
            }
        }
    }
    m_constants.push_back(value);
    return static_cast<u16>(m_constants.size() - 1);
}

u16 Chunk::read_u16(usize offset) const {
    return static_cast<u16>((m_code[offset] << 8) | m_code[offset + 1]);
}

i16 Chunk::read_i16(usize offset) const {
    return static_cast<i16>(read_u16(offset));
}

void Chunk::set_line(usize line) {
    m_current_line = line;
    if (m_lines.empty() || m_lines.back().line != line) {
        m_lines.push_back({m_code.size(), line});
    }
}

usize Chunk::get_line(usize offset) const {
    // Binary search for the line
    usize line = 1;
    for (const auto& info : m_lines) {
        if (info.offset > offset) break;
        line = info.line;
    }
    return line;
}

String Chunk::disassemble() const {
    std::ostringstream oss;
    oss << "=== Chunk Disassembly ===\n";

    usize offset = 0;
    while (offset < m_code.size()) {
        oss << disassemble_instruction(offset).data();

        // Advance offset based on instruction
        auto op = static_cast<OpCode>(m_code[offset++]);
        switch (op) {
            case OpCode::LoadConst:
            case OpCode::GetLocal:
            case OpCode::SetLocal:
            case OpCode::GetGlobal:
            case OpCode::SetGlobal:
            case OpCode::GetUpvalue:
            case OpCode::SetUpvalue:
            case OpCode::DefineGlobal:
            case OpCode::GetProperty:
            case OpCode::SetProperty:
            case OpCode::Call:
            case OpCode::TailCall:
            case OpCode::CreateArray:
            case OpCode::CreateClass:
            case OpCode::DefineMethod:
                offset += 1;
                break;
            case OpCode::Jump:
            case OpCode::JumpIfFalse:
            case OpCode::JumpIfTrue:
            case OpCode::JumpIfFalseKeep:
            case OpCode::JumpIfTrueKeep:
            case OpCode::Loop:
            case OpCode::Closure:
                offset += 2;
                break;
            default:
                break;
        }
    }

    return String(oss.str());
}

String Chunk::disassemble_instruction(usize offset) const {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << offset << " ";

    usize line = get_line(offset);
    if (offset > 0 && line == get_line(offset - 1)) {
        oss << "   | ";
    } else {
        oss << std::setfill(' ') << std::setw(4) << line << " ";
    }

    auto op = static_cast<OpCode>(m_code[offset]);

    auto opcode_name = [](OpCode op) -> const char* {
        switch (op) {
            case OpCode::Pop: return "POP";
            case OpCode::Dup: return "DUP";
            case OpCode::Swap: return "SWAP";
            case OpCode::LoadConst: return "LOAD_CONST";
            case OpCode::LoadNull: return "LOAD_NULL";
            case OpCode::LoadUndefined: return "LOAD_UNDEFINED";
            case OpCode::LoadTrue: return "LOAD_TRUE";
            case OpCode::LoadFalse: return "LOAD_FALSE";
            case OpCode::LoadZero: return "LOAD_ZERO";
            case OpCode::LoadOne: return "LOAD_ONE";
            case OpCode::GetLocal: return "GET_LOCAL";
            case OpCode::SetLocal: return "SET_LOCAL";
            case OpCode::GetGlobal: return "GET_GLOBAL";
            case OpCode::SetGlobal: return "SET_GLOBAL";
            case OpCode::GetUpvalue: return "GET_UPVALUE";
            case OpCode::SetUpvalue: return "SET_UPVALUE";
            case OpCode::DefineGlobal: return "DEFINE_GLOBAL";
            case OpCode::GetProperty: return "GET_PROPERTY";
            case OpCode::SetProperty: return "SET_PROPERTY";
            case OpCode::GetElement: return "GET_ELEMENT";
            case OpCode::SetElement: return "SET_ELEMENT";
            case OpCode::DeleteProperty: return "DELETE_PROPERTY";
            case OpCode::Add: return "ADD";
            case OpCode::Subtract: return "SUBTRACT";
            case OpCode::Multiply: return "MULTIPLY";
            case OpCode::Divide: return "DIVIDE";
            case OpCode::Modulo: return "MODULO";
            case OpCode::Exponent: return "EXPONENT";
            case OpCode::Negate: return "NEGATE";
            case OpCode::Increment: return "INCREMENT";
            case OpCode::Decrement: return "DECREMENT";
            case OpCode::BitwiseNot: return "BITWISE_NOT";
            case OpCode::BitwiseAnd: return "BITWISE_AND";
            case OpCode::BitwiseOr: return "BITWISE_OR";
            case OpCode::BitwiseXor: return "BITWISE_XOR";
            case OpCode::LeftShift: return "LEFT_SHIFT";
            case OpCode::RightShift: return "RIGHT_SHIFT";
            case OpCode::UnsignedRightShift: return "UNSIGNED_RIGHT_SHIFT";
            case OpCode::Equal: return "EQUAL";
            case OpCode::NotEqual: return "NOT_EQUAL";
            case OpCode::StrictEqual: return "STRICT_EQUAL";
            case OpCode::StrictNotEqual: return "STRICT_NOT_EQUAL";
            case OpCode::LessThan: return "LESS_THAN";
            case OpCode::LessEqual: return "LESS_EQUAL";
            case OpCode::GreaterThan: return "GREATER_THAN";
            case OpCode::GreaterEqual: return "GREATER_EQUAL";
            case OpCode::Not: return "NOT";
            case OpCode::Typeof: return "TYPEOF";
            case OpCode::Instanceof: return "INSTANCEOF";
            case OpCode::In: return "IN";
            case OpCode::Jump: return "JUMP";
            case OpCode::JumpIfFalse: return "JUMP_IF_FALSE";
            case OpCode::JumpIfTrue: return "JUMP_IF_TRUE";
            case OpCode::JumpIfFalseKeep: return "JUMP_IF_FALSE_KEEP";
            case OpCode::JumpIfTrueKeep: return "JUMP_IF_TRUE_KEEP";
            case OpCode::Loop: return "LOOP";
            case OpCode::Call: return "CALL";
            case OpCode::TailCall: return "TAIL_CALL";
            case OpCode::Return: return "RETURN";
            case OpCode::Closure: return "CLOSURE";
            case OpCode::CreateObject: return "CREATE_OBJECT";
            case OpCode::CreateArray: return "CREATE_ARRAY";
            case OpCode::DefineProperty: return "DEFINE_PROPERTY";
            case OpCode::CreateClass: return "CREATE_CLASS";
            case OpCode::DefineMethod: return "DEFINE_METHOD";
            case OpCode::GetSuper: return "GET_SUPER";
            case OpCode::GetIterator: return "GET_ITERATOR";
            case OpCode::IteratorNext: return "ITERATOR_NEXT";
            case OpCode::IteratorClose: return "ITERATOR_CLOSE";
            case OpCode::Throw: return "THROW";
            case OpCode::PushExceptionHandler: return "PUSH_EXCEPTION_HANDLER";
            case OpCode::PopExceptionHandler: return "POP_EXCEPTION_HANDLER";
            case OpCode::EnterFinally: return "ENTER_FINALLY";
            case OpCode::LeaveFinally: return "LEAVE_FINALLY";
            case OpCode::This: return "THIS";
            case OpCode::SuperCall: return "SUPER_CALL";
            case OpCode::Spread: return "SPREAD";
            case OpCode::Debugger: return "DEBUGGER";
        }
        return "UNKNOWN";
    };

    oss << opcode_name(op);

    // Print operands
    switch (op) {
        case OpCode::LoadConst: {
            u8 idx = m_code[offset + 1];
            oss << " " << static_cast<int>(idx);
            if (idx < m_constants.size()) {
                oss << " (";
                std::visit([&oss](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, f64>) {
                        oss << arg;
                    } else if constexpr (std::is_same_v<T, String>) {
                        oss << "\"" << arg.data() << "\"";
                    } else if constexpr (std::is_same_v<T, bool>) {
                        oss << (arg ? "true" : "false");
                    }
                }, m_constants[idx]);
                oss << ")";
            }
            break;
        }
        case OpCode::GetLocal:
        case OpCode::SetLocal:
        case OpCode::GetUpvalue:
        case OpCode::SetUpvalue:
        case OpCode::Call:
        case OpCode::TailCall:
        case OpCode::CreateArray:
            oss << " " << static_cast<int>(m_code[offset + 1]);
            break;
        case OpCode::Jump:
        case OpCode::JumpIfFalse:
        case OpCode::JumpIfTrue:
        case OpCode::JumpIfFalseKeep:
        case OpCode::JumpIfTrueKeep: {
            i16 jump = read_i16(offset + 1);
            oss << " " << jump << " -> " << (offset + 3 + jump);
            break;
        }
        case OpCode::Loop: {
            u16 loop = read_u16(offset + 1);
            oss << " " << loop << " -> " << (offset + 3 - loop);
            break;
        }
        default:
            break;
    }

    oss << "\n";
    return String(oss.str());
}

// ============================================================================
// Function implementation
// ============================================================================

Function::Function(String name, u8 arity)
    : m_name(std::move(name))
    , m_arity(arity)
{
}

void Function::add_upvalue(u8 index, bool is_local) {
    m_upvalues.push_back({index, is_local});
}

} // namespace lithium::js
