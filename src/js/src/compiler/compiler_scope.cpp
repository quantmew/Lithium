/**
 * JavaScript Compiler - scope management helpers
 */

#include "lithium/js/compiler.hpp"

namespace lithium::js {

void Compiler::begin_scope() {
    m_current->scope_depth++;
}

void Compiler::end_scope() {
    m_current->scope_depth--;

    // Pop locals that went out of scope
    while (!m_current->locals.empty() &&
           m_current->locals.back().depth > m_current->scope_depth) {
        if (m_current->locals.back().is_captured) {
            // Close upvalue
            emit(OpCode::Pop); // Simplified - full impl would use CloseUpvalue
        } else {
            emit(OpCode::Pop);
        }
        m_current->locals.pop_back();
    }
}

void Compiler::declare_variable(const String& name, bool is_const) {
    if (m_current->scope_depth == 0) {
        return; // Global - nothing to declare
    }

    // Check for duplicate in current scope
    for (auto it = m_current->locals.rbegin(); it != m_current->locals.rend(); ++it) {
        if (it->depth != -1 && it->depth < m_current->scope_depth) {
            break;
        }
        if (it->name == name) {
            error("Variable '"_s + name + "' already declared in this scope"_s);
            return;
        }
    }

    add_local(name, is_const);
}

void Compiler::define_variable(u16 global_idx) {
    if (m_current->scope_depth > 0) {
        mark_initialized();
        return;
    }
    emit(OpCode::DefineGlobal);
    emit(static_cast<u8>(global_idx));
}

u16 Compiler::identifier_constant(const String& name) {
    return make_constant(name);
}

i32 Compiler::resolve_local(CompilerState* state, const String& name) {
    for (i32 i = static_cast<i32>(state->locals.size()) - 1; i >= 0; --i) {
        if (state->locals[i].name == name) {
            if (state->locals[i].depth == -1) {
                error("Cannot read variable in its own initializer"_s);
            }
            return i;
        }
    }
    return -1;
}

i32 Compiler::resolve_upvalue(CompilerState* state, const String& name) {
    if (!state->enclosing) {
        return -1;
    }

    i32 local = resolve_local(state->enclosing, name);
    if (local != -1) {
        state->enclosing->locals[local].is_captured = true;
        for (usize i = 0; i < state->upvalues.size(); ++i) {
            if (state->upvalues[i].index == static_cast<u8>(local) &&
                state->upvalues[i].is_local) {
                return static_cast<i32>(i);
            }
        }
        state->upvalues.push_back({static_cast<u8>(local), true});
        return static_cast<i32>(state->upvalues.size() - 1);
    }

    i32 upvalue = resolve_upvalue(state->enclosing, name);
    if (upvalue != -1) {
        for (usize i = 0; i < state->upvalues.size(); ++i) {
            if (state->upvalues[i].index == static_cast<u8>(upvalue) &&
                !state->upvalues[i].is_local) {
                return static_cast<i32>(i);
            }
        }
        state->upvalues.push_back({static_cast<u8>(upvalue), false});
        return static_cast<i32>(state->upvalues.size() - 1);
    }

    return -1;
}

void Compiler::add_local(const String& name, bool is_const) {
    if (m_current->locals.size() >= 256) {
        error("Too many local variables in function"_s);
        return;
    }
    m_current->locals.push_back({name, -1, false, is_const});
}

void Compiler::mark_initialized() {
    if (m_current->scope_depth == 0) return;
    m_current->locals.back().depth = m_current->scope_depth;
}

} // namespace lithium::js
