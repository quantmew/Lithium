/**
 * JavaScript Compiler - destructuring patterns and function bodies
 */

#include "lithium/js/compiler.hpp"

namespace lithium::js {

void Compiler::compile_pattern_assignment(const Pattern& pattern) {
    if (auto* array_pattern = dynamic_cast<const ArrayPattern*>(&pattern)) {
        compile_array_pattern(*array_pattern);
    } else if (auto* obj_pattern = dynamic_cast<const ObjectPattern*>(&pattern)) {
        compile_object_pattern(*obj_pattern);
    }
}

void Compiler::compile_array_pattern(const ArrayPattern& pattern) {
    for (usize i = 0; i < pattern.elements.size(); ++i) {
        if (!pattern.elements[i]) continue;

        emit(OpCode::Dup);
        emit_constant(static_cast<f64>(i));
        emit(OpCode::GetElement);

        if (auto* id = dynamic_cast<const Identifier*>(pattern.elements[i].get())) {
            declare_variable(id->name);
            mark_initialized();
        }
    }
    emit(OpCode::Pop);
}

void Compiler::compile_object_pattern(const ObjectPattern& pattern) {
    for (const auto& prop : pattern.properties) {
        emit(OpCode::Dup);

        if (auto* key = dynamic_cast<const Identifier*>(prop.key.get())) {
            u16 idx = identifier_constant(key->name);
            emit(OpCode::GetProperty);
            emit(static_cast<u8>(idx));

            if (auto* value_id = dynamic_cast<const Identifier*>(prop.value.get())) {
                declare_variable(value_id->name);
                mark_initialized();
            }
        }
    }
    emit(OpCode::Pop);
}

void Compiler::compile_function_body(const String& name,
                                     const std::vector<PatternPtr>& params,
                                     const Statement& body,
                                     CompilerState::Type type) {
    auto function = std::make_shared<Function>(name, static_cast<u8>(params.size()));
    CompilerState state;
    state.enclosing = m_current;
    state.function = function;
    state.type = type;
    m_current = &state;

    begin_scope();

    state.locals.push_back({"", 0, false, false});

    for (const auto& param : params) {
        if (auto* id = dynamic_cast<const Identifier*>(param.get())) {
            declare_variable(id->name);
            mark_initialized();
        }
    }

    if (auto* block = dynamic_cast<const BlockStatement*>(&body)) {
        for (const auto& stmt : block->body) {
            compile_statement(*stmt);
        }
    } else if (auto* expr = dynamic_cast<const Expression*>(&body)) {
        compile_expression(*expr);
        emit(OpCode::Return);
    }

    if (type == CompilerState::Type::Initializer) {
        emit(OpCode::GetLocal);
        emit(0);
    } else {
        emit(OpCode::LoadUndefined);
    }
    emit(OpCode::Return);

    end_scope();

    m_current = state.enclosing;
    function->set_local_count(static_cast<u8>(state.locals.size()));

    u16 func_idx = current_chunk().add_constant(name);
    emit(OpCode::Closure);
    emit(static_cast<u8>(func_idx));
    emit(static_cast<u8>(state.upvalues.size()));

    for (const auto& upvalue : state.upvalues) {
        emit(upvalue.is_local ? 1 : 0);
        emit(upvalue.index);
    }
}

} // namespace lithium::js
