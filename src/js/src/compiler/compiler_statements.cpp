/**
 * JavaScript Compiler - statement compilation
 */

#include "lithium/js/compiler.hpp"

namespace lithium::js {

void Compiler::compile_statement(const Statement& stmt) {
    if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(&stmt)) {
        compile_expression_statement(*expr_stmt);
    } else if (auto* block = dynamic_cast<const BlockStatement*>(&stmt)) {
        compile_block_statement(*block);
    } else if (auto* var_decl = dynamic_cast<const VariableDeclaration*>(&stmt)) {
        compile_variable_declaration(*var_decl);
    } else if (auto* func_decl = dynamic_cast<const FunctionDeclaration*>(&stmt)) {
        compile_function_declaration(*func_decl);
    } else if (auto* class_decl = dynamic_cast<const ClassDeclaration*>(&stmt)) {
        compile_class_declaration(*class_decl);
    } else if (auto* if_stmt = dynamic_cast<const IfStatement*>(&stmt)) {
        compile_if_statement(*if_stmt);
    } else if (auto* while_stmt = dynamic_cast<const WhileStatement*>(&stmt)) {
        compile_while_statement(*while_stmt);
    } else if (auto* do_while = dynamic_cast<const DoWhileStatement*>(&stmt)) {
        compile_do_while_statement(*do_while);
    } else if (auto* for_stmt = dynamic_cast<const ForStatement*>(&stmt)) {
        compile_for_statement(*for_stmt);
    } else if (auto* for_in = dynamic_cast<const ForInStatement*>(&stmt)) {
        compile_for_in_statement(*for_in);
    } else if (auto* for_of = dynamic_cast<const ForOfStatement*>(&stmt)) {
        compile_for_of_statement(*for_of);
    } else if (auto* switch_stmt = dynamic_cast<const SwitchStatement*>(&stmt)) {
        compile_switch_statement(*switch_stmt);
    } else if (auto* try_stmt = dynamic_cast<const TryStatement*>(&stmt)) {
        compile_try_statement(*try_stmt);
    } else if (auto* ret = dynamic_cast<const ReturnStatement*>(&stmt)) {
        compile_return_statement(*ret);
    } else if (auto* throw_stmt = dynamic_cast<const ThrowStatement*>(&stmt)) {
        compile_throw_statement(*throw_stmt);
    } else if (auto* brk = dynamic_cast<const BreakStatement*>(&stmt)) {
        compile_break_statement(*brk);
    } else if (auto* cont = dynamic_cast<const ContinueStatement*>(&stmt)) {
        compile_continue_statement(*cont);
    } else if (dynamic_cast<const EmptyStatement*>(&stmt)) {
        // Nothing to compile
    } else {
        error("Unknown statement type"_s);
    }
}

void Compiler::compile_expression_statement(const ExpressionStatement& stmt) {
    compile_expression(*stmt.expression);
    emit(OpCode::Pop);
}

void Compiler::compile_block_statement(const BlockStatement& stmt) {
    begin_scope();
    for (const auto& s : stmt.body) {
        compile_statement(*s);
    }
    end_scope();
}

void Compiler::compile_variable_declaration(const VariableDeclaration& decl) {
    bool is_const = decl.kind == VariableDeclaration::Kind::Const;

    for (const auto& declarator : decl.declarations) {
        if (auto* id = dynamic_cast<const Identifier*>(declarator.id.get())) {
            u16 global = 0;
            if (m_current->scope_depth == 0) {
                global = identifier_constant(id->name);
            } else {
                declare_variable(id->name, is_const);
            }

            if (declarator.init) {
                compile_expression(*declarator.init);
            } else {
                emit(OpCode::LoadUndefined);
            }

            if (m_current->scope_depth == 0) {
                define_variable(global);
            } else {
                mark_initialized();
            }
        }
    }
}

void Compiler::compile_function_declaration(const FunctionDeclaration& decl) {
    u16 name_constant = identifier_constant(decl.id->name);
    if (m_current->scope_depth > 0) {
        declare_variable(decl.id->name, true);
    }

    compile_function_body(decl.id->name, decl.params, *decl.body, CompilerState::Type::Function);

    if (m_current->scope_depth == 0) {
        define_variable(name_constant);
    } else {
        mark_initialized();
    }
}

void Compiler::compile_class_declaration(const ClassDeclaration& decl) {
    u16 name_constant = identifier_constant(decl.id->name);

    if (m_current->scope_depth > 0) {
        declare_variable(decl.id->name, true);
    }

    emit(OpCode::Class);
    emit(static_cast<u8>(name_constant));

    if (m_current->scope_depth == 0) {
        define_variable(name_constant);
    } else {
        mark_initialized();
    }

    begin_scope();
    add_local("this"_s);
    mark_initialized();

    for (const auto& method : decl.body) {
        if (auto* func = dynamic_cast<const FunctionExpression*>(method.get())) {
            compile_function_body(func->id ? func->id->name : ""_s, func->params, *func->body,
                                  func->kind == FunctionExpression::Kind::Constructor
                                      ? CompilerState::Type::Initializer
                                      : CompilerState::Type::Method);
        }
    }

    end_scope();
}

void Compiler::compile_if_statement(const IfStatement& stmt) {
    compile_expression(*stmt.test);
    usize jump_to_else = emit_jump(OpCode::JumpIfFalse);
    emit(OpCode::Pop);
    compile_statement(*stmt.consequent);
    usize jump_to_end = emit_jump(OpCode::Jump);
    patch_jump(jump_to_else);
    emit(OpCode::Pop);
    if (stmt.alternate) {
        compile_statement(*stmt.alternate);
    }
    patch_jump(jump_to_end);
}

void Compiler::compile_while_statement(const WhileStatement& stmt) {
    usize loop_start = current_chunk().size();
    push_loop(loop_start);

    compile_expression(*stmt.test);
    usize exit_jump = emit_jump(OpCode::JumpIfFalse);
    emit(OpCode::Pop);
    compile_statement(*stmt.body);
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit(OpCode::Pop);

    pop_loop();
}

void Compiler::compile_do_while_statement(const DoWhileStatement& stmt) {
    usize loop_start = current_chunk().size();
    push_loop(loop_start);

    compile_statement(*stmt.body);
    compile_expression(*stmt.test);
    emit(OpCode::JumpIfTrue);
    emit_u16(static_cast<u16>(current_chunk().size() - loop_start + 2));

    pop_loop();
}

void Compiler::compile_for_statement(const ForStatement& stmt) {
    begin_scope();

    if (stmt.init) {
        compile_statement(*stmt.init);
    }

    usize loop_start = current_chunk().size();
    push_loop(loop_start);

    usize exit_jump = 0;
    if (stmt.test) {
        compile_expression(*stmt.test);
        exit_jump = emit_jump(OpCode::JumpIfFalse);
        emit(OpCode::Pop);
    }

    compile_statement(*stmt.body);

    if (stmt.update) {
        compile_expression(*stmt.update);
        emit(OpCode::Pop);
    }

    emit_loop(loop_start);

    if (exit_jump) {
        patch_jump(exit_jump);
        emit(OpCode::Pop);
    }

    pop_loop();
    end_scope();
}

void Compiler::compile_for_in_statement(const ForInStatement& stmt) {
    begin_scope();

    compile_expression(*stmt.right);
    emit(OpCode::ForIn);
    emit(static_cast<u8>(stmt.left.size()));

    for (const auto& left : stmt.left) {
        if (auto* id = dynamic_cast<const Identifier*>(left.get())) {
            declare_variable(id->name);
            mark_initialized();
        }
    }

    compile_statement(*stmt.body);

    end_scope();
}

void Compiler::compile_for_of_statement(const ForOfStatement& stmt) {
    begin_scope();

    compile_expression(*stmt.right);
    emit(OpCode::ForOf);
    emit(static_cast<u8>(stmt.left.size()));

    for (const auto& left : stmt.left) {
        if (auto* id = dynamic_cast<const Identifier*>(left.get())) {
            declare_variable(id->name);
            mark_initialized();
        }
    }

    compile_statement(*stmt.body);

    end_scope();
}

void Compiler::compile_switch_statement(const SwitchStatement& stmt) {
    compile_expression(*stmt.discriminant);

    std::vector<usize> jump_to_next_case;
    usize jump_to_end = 0;

    for (const auto& case_stmt : stmt.cases) {
        if (case_stmt->test) {
            emit(OpCode::Dup);
            compile_expression(**case_stmt->test);
            emit(OpCode::StrictEqual);
            jump_to_next_case.push_back(emit_jump(OpCode::JumpIfFalse));
            emit(OpCode::Pop);
        }

        for (const auto& cons : case_stmt->consequent) {
            compile_statement(*cons);
        }

        if (case_stmt->test) {
            jump_to_end = emit_jump(OpCode::Jump);
            patch_jump(jump_to_next_case.back());
            emit(OpCode::Pop);
        }
    }

    if (jump_to_end) {
        patch_jump(jump_to_end);
    }
}

void Compiler::compile_try_statement(const TryStatement& stmt) {
    compile_statement(*stmt.block);
    if (stmt.handler) {
        begin_scope();
        if (stmt.handler->param) {
            if (auto* id = dynamic_cast<const Identifier*>(stmt.handler->param.get())) {
                declare_variable(id->name);
                mark_initialized();
            }
        }
        compile_statement(*stmt.handler->body);
        end_scope();
    }
    if (stmt.finalizer) {
        compile_statement(*stmt.finalizer);
    }
}

void Compiler::compile_return_statement(const ReturnStatement& stmt) {
    if (m_current->type == CompilerState::Type::Initializer) {
        error("Cannot return from initializer"_s);
    }

    if (stmt.argument) {
        compile_expression(*stmt.argument);
    } else {
        emit(OpCode::LoadUndefined);
    }
    emit(OpCode::Return);
}

void Compiler::compile_throw_statement(const ThrowStatement& stmt) {
    compile_expression(*stmt.argument);
    emit(OpCode::Throw);
}

void Compiler::compile_break_statement(const BreakStatement&) {
    emit_break();
}

void Compiler::compile_continue_statement(const ContinueStatement&) {
    emit_continue();
}

void Compiler::push_loop(usize start) {
    m_loop_stack.push_back({start, {}, {}, m_current->scope_depth});
}

void Compiler::pop_loop() {
    auto& loop = m_loop_stack.back();

    for (usize jump : loop.break_jumps) {
        patch_jump(jump);
    }

    m_loop_stack.pop_back();
}

void Compiler::emit_break() {
    if (m_loop_stack.empty()) {
        error("Cannot use 'break' outside of a loop"_s);
        return;
    }

    auto& loop = m_loop_stack.back();
    for (i32 i = static_cast<i32>(m_current->locals.size()) - 1;
         i >= 0 && m_current->locals[i].depth > loop.scope_depth; --i) {
        emit(OpCode::Pop);
    }

    loop.break_jumps.push_back(emit_jump(OpCode::Jump));
}

void Compiler::emit_continue() {
    if (m_loop_stack.empty()) {
        error("Cannot use 'continue' outside of a loop"_s);
        return;
    }

    auto& loop = m_loop_stack.back();
    for (i32 i = static_cast<i32>(m_current->locals.size()) - 1;
         i >= 0 && m_current->locals[i].depth > loop.scope_depth; --i) {
        emit(OpCode::Pop);
    }

    loop.continue_jumps.push_back(emit_jump(OpCode::Jump));
}

} // namespace lithium::js
