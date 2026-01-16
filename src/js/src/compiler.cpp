/**
 * JavaScript Compiler - AST to Bytecode
 */

#include "lithium/js/compiler.hpp"
#include "lithium/js/parser.hpp"

namespace lithium::js {

// ============================================================================
// Compiler implementation
// ============================================================================

Compiler::Compiler() = default;

std::shared_ptr<Function> Compiler::compile(const Program& program) {
    m_errors.clear();
    m_had_error = false;
    m_panic_mode = false;

    // Create top-level function
    auto function = std::make_shared<Function>(""_s, 0);
    CompilerState state;
    state.function = function;
    state.type = CompilerState::Type::Script;
    m_current = &state;

    // Add implicit local for script (slot 0)
    state.locals.push_back({"", 0, false, false});

    // Compile all statements
    for (const auto& stmt : program.body) {
        compile_statement(*stmt);
        if (m_had_error && m_panic_mode) {
            break;
        }
    }

    // Emit return
    emit(OpCode::LoadUndefined);
    emit(OpCode::Return);

    m_current = nullptr;

    if (m_had_error) {
        return nullptr;
    }

    function->set_local_count(static_cast<u8>(state.locals.size()));
    return function;
}

std::shared_ptr<Function> Compiler::compile_expression(const Expression& expr) {
    m_errors.clear();
    m_had_error = false;

    auto function = std::make_shared<Function>("(expr)"_s, 0);
    CompilerState state;
    state.function = function;
    state.type = CompilerState::Type::Script;
    m_current = &state;

    state.locals.push_back({"", 0, false, false});

    compile_expression(expr);
    emit(OpCode::Return);

    m_current = nullptr;

    if (m_had_error) {
        return nullptr;
    }

    function->set_local_count(static_cast<u8>(state.locals.size()));
    return function;
}

// ============================================================================
// Bytecode emission
// ============================================================================

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

// ============================================================================
// Scope management
// ============================================================================

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
        // Add upvalue
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

// ============================================================================
// Statement compilation
// ============================================================================

void Compiler::compile_statement(const Statement& stmt) {
    // Use visitor pattern or type checking
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
        // Handle simple identifier pattern
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

            define_variable(global);
        } else {
            // Handle destructuring patterns
            if (declarator.init) {
                compile_expression(*declarator.init);
                compile_pattern_assignment(*declarator.id);
            }
        }
    }
}

void Compiler::compile_function_declaration(const FunctionDeclaration& decl) {
    u16 global = identifier_constant(decl.id->name);

    if (m_current->scope_depth > 0) {
        declare_variable(decl.id->name);
    }

    compile_function_body(decl.id->name, decl.params, *decl.body,
                          CompilerState::Type::Function);

    define_variable(global);
}

void Compiler::compile_class_declaration(const ClassDeclaration& decl) {
    u16 name_constant = identifier_constant(decl.id->name);
    declare_variable(decl.id->name);

    emit(OpCode::CreateClass);
    emit(static_cast<u8>(name_constant));

    define_variable(name_constant);

    // Compile methods
    for (const auto& method : decl.body->body) {
        if (auto* method_def = dynamic_cast<const MethodDefinition*>(method.get())) {
            auto* key = dynamic_cast<const Identifier*>(method_def->key.get());
            if (!key) continue;

            u16 method_name = identifier_constant(key->name);
            compile_function_body(key->name, method_def->value->params,
                                  *method_def->value->body,
                                  method_def->kind == MethodDefinition::Kind::Constructor
                                      ? CompilerState::Type::Initializer
                                      : CompilerState::Type::Method);

            emit(OpCode::DefineMethod);
            emit(static_cast<u8>(method_name));
        }
    }

    emit(OpCode::Pop);
}

void Compiler::compile_if_statement(const IfStatement& stmt) {
    compile_expression(*stmt.test);

    usize then_jump = emit_jump(OpCode::JumpIfFalse);
    emit(OpCode::Pop);

    compile_statement(*stmt.consequent);

    usize else_jump = emit_jump(OpCode::Jump);
    patch_jump(then_jump);
    emit(OpCode::Pop);

    if (stmt.alternate) {
        compile_statement(*stmt.alternate);
    }
    patch_jump(else_jump);
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
    usize exit_jump = emit_jump(OpCode::JumpIfFalse);
    emit(OpCode::Pop);
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit(OpCode::Pop);

    pop_loop();
}

void Compiler::compile_for_statement(const ForStatement& stmt) {
    begin_scope();

    if (stmt.init) {
        if (auto* var_decl = dynamic_cast<const VariableDeclaration*>(stmt.init.get())) {
            compile_variable_declaration(*var_decl);
        } else if (auto* expr = dynamic_cast<const Expression*>(stmt.init.get())) {
            compile_expression(*expr);
            emit(OpCode::Pop);
        }
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

    usize increment_start = current_chunk().size();
    if (stmt.update) {
        compile_expression(*stmt.update);
        emit(OpCode::Pop);
    }

    emit_loop(loop_start);

    if (stmt.test) {
        patch_jump(exit_jump);
        emit(OpCode::Pop);
    }

    // Patch continue jumps to increment
    for (usize jump : m_loop_stack.back().continue_jumps) {
        current_chunk().patch_jump(jump, static_cast<i16>(increment_start - jump - 2));
    }

    pop_loop();
    end_scope();
}

void Compiler::compile_for_in_statement(const ForInStatement& stmt) {
    // Simplified for-in compilation
    begin_scope();

    compile_expression(*stmt.right);
    emit(OpCode::GetIterator);

    usize loop_start = current_chunk().size();
    push_loop(loop_start);

    emit(OpCode::IteratorNext);
    usize exit_jump = emit_jump(OpCode::JumpIfFalse);

    // Bind loop variable
    if (auto* var_decl = dynamic_cast<const VariableDeclaration*>(stmt.left.get())) {
        compile_variable_declaration(*var_decl);
    }

    compile_statement(*stmt.body);
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit(OpCode::IteratorClose);

    pop_loop();
    end_scope();
}

void Compiler::compile_for_of_statement(const ForOfStatement& stmt) {
    // Simplified for-of compilation - similar to for-in
    compile_for_in_statement(ForInStatement{
        stmt.left ? stmt.left->clone() : nullptr,
        stmt.right ? stmt.right->clone() : nullptr,
        stmt.body ? stmt.body->clone() : nullptr
    });
}

void Compiler::compile_switch_statement(const SwitchStatement& stmt) {
    compile_expression(*stmt.discriminant);

    std::vector<usize> case_ends;
    usize default_jump = 0;
    bool has_default = false;

    for (const auto& case_clause : stmt.cases) {
        if (case_clause.test) {
            emit(OpCode::Dup);
            compile_expression(*case_clause.test);
            emit(OpCode::StrictEqual);
            usize next_case = emit_jump(OpCode::JumpIfFalse);
            emit(OpCode::Pop);

            for (const auto& s : case_clause.consequent) {
                compile_statement(*s);
            }
            case_ends.push_back(emit_jump(OpCode::Jump));

            patch_jump(next_case);
            emit(OpCode::Pop);
        } else {
            // Default case
            has_default = true;
            default_jump = emit_jump(OpCode::Jump);
        }
    }

    if (has_default) {
        patch_jump(default_jump);
        for (const auto& case_clause : stmt.cases) {
            if (!case_clause.test) {
                for (const auto& s : case_clause.consequent) {
                    compile_statement(*s);
                }
                break;
            }
        }
    }

    for (usize end : case_ends) {
        patch_jump(end);
    }

    emit(OpCode::Pop); // Pop discriminant
}

void Compiler::compile_try_statement(const TryStatement& stmt) {
    usize handler_offset = emit_jump(OpCode::PushExceptionHandler);

    compile_statement(*stmt.block);
    emit(OpCode::PopExceptionHandler);
    usize end_try = emit_jump(OpCode::Jump);

    patch_jump(handler_offset);

    if (stmt.handler) {
        begin_scope();
        if (auto* param = dynamic_cast<const Identifier*>(stmt.handler->param.get())) {
            declare_variable(param->name);
            mark_initialized();
            // Exception is on stack
        }
        compile_statement(*stmt.handler->body);
        end_scope();
    }

    patch_jump(end_try);

    if (stmt.finalizer) {
        compile_statement(*stmt.finalizer);
    }
}

void Compiler::compile_return_statement(const ReturnStatement& stmt) {
    if (m_current->type == CompilerState::Type::Script) {
        error("Cannot return from top-level code"_s);
        return;
    }

    if (stmt.argument) {
        if (m_current->type == CompilerState::Type::Initializer) {
            error("Cannot return a value from an initializer"_s);
            return;
        }
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

// ============================================================================
// Expression compilation
// ============================================================================

void Compiler::compile_expression(const Expression& expr) {
    if (auto* lit = dynamic_cast<const NullLiteral*>(&expr)) {
        compile_literal(*lit);
    } else if (auto* lit = dynamic_cast<const BooleanLiteral*>(&expr)) {
        compile_literal(*lit);
    } else if (auto* lit = dynamic_cast<const NumericLiteral*>(&expr)) {
        compile_literal(*lit);
    } else if (auto* lit = dynamic_cast<const StringLiteral*>(&expr)) {
        compile_literal(*lit);
    } else if (auto* tmpl = dynamic_cast<const TemplateLiteral*>(&expr)) {
        compile_template_literal(*tmpl);
    } else if (auto* id = dynamic_cast<const Identifier*>(&expr)) {
        compile_identifier(*id);
    } else if (auto* this_expr = dynamic_cast<const ThisExpression*>(&expr)) {
        compile_this_expression(*this_expr);
    } else if (auto* arr = dynamic_cast<const ArrayExpression*>(&expr)) {
        compile_array_expression(*arr);
    } else if (auto* obj = dynamic_cast<const ObjectExpression*>(&expr)) {
        compile_object_expression(*obj);
    } else if (auto* func = dynamic_cast<const FunctionExpression*>(&expr)) {
        compile_function_expression(*func);
    } else if (auto* arrow = dynamic_cast<const ArrowFunctionExpression*>(&expr)) {
        compile_arrow_function(*arrow);
    } else if (auto* cls = dynamic_cast<const ClassExpression*>(&expr)) {
        compile_class_expression(*cls);
    } else if (auto* member = dynamic_cast<const MemberExpression*>(&expr)) {
        compile_member_expression(*member);
    } else if (auto* call = dynamic_cast<const CallExpression*>(&expr)) {
        compile_call_expression(*call);
    } else if (auto* new_expr = dynamic_cast<const NewExpression*>(&expr)) {
        compile_new_expression(*new_expr);
    } else if (auto* unary = dynamic_cast<const UnaryExpression*>(&expr)) {
        compile_unary_expression(*unary);
    } else if (auto* update = dynamic_cast<const UpdateExpression*>(&expr)) {
        compile_update_expression(*update);
    } else if (auto* binary = dynamic_cast<const BinaryExpression*>(&expr)) {
        compile_binary_expression(*binary);
    } else if (auto* logical = dynamic_cast<const LogicalExpression*>(&expr)) {
        compile_logical_expression(*logical);
    } else if (auto* cond = dynamic_cast<const ConditionalExpression*>(&expr)) {
        compile_conditional_expression(*cond);
    } else if (auto* assign = dynamic_cast<const AssignmentExpression*>(&expr)) {
        compile_assignment_expression(*assign);
    } else if (auto* seq = dynamic_cast<const SequenceExpression*>(&expr)) {
        compile_sequence_expression(*seq);
    } else {
        error("Unknown expression type"_s);
    }
}

void Compiler::compile_literal(const NullLiteral&) {
    emit(OpCode::LoadNull);
}

void Compiler::compile_literal(const BooleanLiteral& lit) {
    emit(lit.value ? OpCode::LoadTrue : OpCode::LoadFalse);
}

void Compiler::compile_literal(const NumericLiteral& lit) {
    if (lit.value == 0) {
        emit(OpCode::LoadZero);
    } else if (lit.value == 1) {
        emit(OpCode::LoadOne);
    } else {
        emit_constant(lit.value);
    }
}

void Compiler::compile_literal(const StringLiteral& lit) {
    emit_constant(lit.value);
}

void Compiler::compile_template_literal(const TemplateLiteral& lit) {
    // Compile quasi-expression pairs
    bool first = true;
    for (usize i = 0; i < lit.quasis.size(); ++i) {
        if (!lit.quasis[i].value.empty()) {
            emit_constant(lit.quasis[i].value);
            if (!first) {
                emit(OpCode::Add);
            }
            first = false;
        }

        if (i < lit.expressions.size()) {
            compile_expression(*lit.expressions[i]);
            // Convert to string and concatenate
            if (!first) {
                emit(OpCode::Add);
            }
            first = false;
        }
    }

    if (first) {
        // Empty template
        emit_constant(""_s);
    }
}

void Compiler::compile_identifier(const Identifier& id) {
    // Check for special identifiers
    if (id.name == "undefined"_s) {
        emit(OpCode::LoadUndefined);
        return;
    }

    i32 local = resolve_local(m_current, id.name);
    if (local != -1) {
        emit(OpCode::GetLocal);
        emit(static_cast<u8>(local));
    } else {
        i32 upvalue = resolve_upvalue(m_current, id.name);
        if (upvalue != -1) {
            emit(OpCode::GetUpvalue);
            emit(static_cast<u8>(upvalue));
        } else {
            u16 idx = identifier_constant(id.name);
            emit(OpCode::GetGlobal);
            emit(static_cast<u8>(idx));
        }
    }
}

void Compiler::compile_this_expression(const ThisExpression&) {
    emit(OpCode::This);
}

void Compiler::compile_array_expression(const ArrayExpression& expr) {
    for (const auto& elem : expr.elements) {
        if (elem) {
            compile_expression(*elem);
        } else {
            emit(OpCode::LoadUndefined);
        }
    }
    emit(OpCode::CreateArray);
    emit(static_cast<u8>(expr.elements.size()));
}

void Compiler::compile_object_expression(const ObjectExpression& expr) {
    emit(OpCode::CreateObject);

    for (const auto& prop : expr.properties) {
        emit(OpCode::Dup);

        // Get property key
        if (auto* key = dynamic_cast<const Identifier*>(prop.key.get())) {
            emit_constant(key->name);
        } else if (auto* lit = dynamic_cast<const StringLiteral*>(prop.key.get())) {
            emit_constant(lit->value);
        } else {
            compile_expression(*prop.key);
        }

        compile_expression(*prop.value);
        emit(OpCode::SetProperty);
        emit(OpCode::Pop);
    }
}

void Compiler::compile_function_expression(const FunctionExpression& expr) {
    String name = expr.id ? expr.id->name : ""_s;
    compile_function_body(name, expr.params, *expr.body, CompilerState::Type::Function);
}

void Compiler::compile_arrow_function(const ArrowFunctionExpression& expr) {
    compile_function_body(""_s, expr.params, *expr.body, CompilerState::Type::Function);
}

void Compiler::compile_class_expression(const ClassExpression& expr) {
    String name = expr.id ? expr.id->name : ""_s;
    u16 name_constant = identifier_constant(name);

    emit(OpCode::CreateClass);
    emit(static_cast<u8>(name_constant));

    // Compile methods
    for (const auto& method : expr.body->body) {
        if (auto* method_def = dynamic_cast<const MethodDefinition*>(method.get())) {
            auto* key = dynamic_cast<const Identifier*>(method_def->key.get());
            if (!key) continue;

            u16 method_name = identifier_constant(key->name);
            compile_function_body(key->name, method_def->value->params,
                                  *method_def->value->body,
                                  CompilerState::Type::Method);

            emit(OpCode::DefineMethod);
            emit(static_cast<u8>(method_name));
        }
    }
}

void Compiler::compile_member_expression(const MemberExpression& expr) {
    compile_expression(*expr.object);

    if (expr.computed) {
        compile_expression(*expr.property);
        emit(OpCode::GetElement);
    } else {
        if (auto* prop = dynamic_cast<const Identifier*>(expr.property.get())) {
            u16 idx = identifier_constant(prop->name);
            emit(OpCode::GetProperty);
            emit(static_cast<u8>(idx));
        }
    }
}

void Compiler::compile_call_expression(const CallExpression& expr) {
    compile_expression(*expr.callee);

    for (const auto& arg : expr.arguments) {
        compile_expression(*arg);
    }

    emit(OpCode::Call);
    emit(static_cast<u8>(expr.arguments.size()));
}

void Compiler::compile_new_expression(const NewExpression& expr) {
    compile_expression(*expr.callee);

    for (const auto& arg : expr.arguments) {
        compile_expression(*arg);
    }

    emit(OpCode::Call);  // Simplified - full impl would use New opcode
    emit(static_cast<u8>(expr.arguments.size()));
}

void Compiler::compile_unary_expression(const UnaryExpression& expr) {
    compile_expression(*expr.argument);

    switch (expr.op) {
        case UnaryExpression::Operator::Minus:
            emit(OpCode::Negate);
            break;
        case UnaryExpression::Operator::Plus:
            // ToNumber - simplified, just leave value
            break;
        case UnaryExpression::Operator::Not:
            emit(OpCode::Not);
            break;
        case UnaryExpression::Operator::BitwiseNot:
            emit(OpCode::BitwiseNot);
            break;
        case UnaryExpression::Operator::Typeof:
            emit(OpCode::Typeof);
            break;
        case UnaryExpression::Operator::Void:
            emit(OpCode::Pop);
            emit(OpCode::LoadUndefined);
            break;
        case UnaryExpression::Operator::Delete:
            emit(OpCode::DeleteProperty);
            break;
    }
}

void Compiler::compile_update_expression(const UpdateExpression& expr) {
    // Get current value
    compile_expression(*expr.argument);

    if (!expr.prefix) {
        emit(OpCode::Dup);
    }

    // Increment/decrement
    emit(expr.op == UpdateExpression::Operator::Increment
         ? OpCode::Increment : OpCode::Decrement);

    if (expr.prefix) {
        emit(OpCode::Dup);
    }

    // Store back - simplified
    if (auto* id = dynamic_cast<const Identifier*>(expr.argument.get())) {
        i32 local = resolve_local(m_current, id->name);
        if (local != -1) {
            emit(OpCode::SetLocal);
            emit(static_cast<u8>(local));
        } else {
            u16 idx = identifier_constant(id->name);
            emit(OpCode::SetGlobal);
            emit(static_cast<u8>(idx));
        }
    }

    if (!expr.prefix) {
        emit(OpCode::Swap);
        emit(OpCode::Pop);
    }
}

void Compiler::compile_binary_expression(const BinaryExpression& expr) {
    compile_expression(*expr.left);
    compile_expression(*expr.right);

    switch (expr.op) {
        case BinaryExpression::Operator::Add:
            emit(OpCode::Add);
            break;
        case BinaryExpression::Operator::Subtract:
            emit(OpCode::Subtract);
            break;
        case BinaryExpression::Operator::Multiply:
            emit(OpCode::Multiply);
            break;
        case BinaryExpression::Operator::Divide:
            emit(OpCode::Divide);
            break;
        case BinaryExpression::Operator::Modulo:
            emit(OpCode::Modulo);
            break;
        case BinaryExpression::Operator::Exponent:
            emit(OpCode::Exponent);
            break;
        case BinaryExpression::Operator::Equal:
            emit(OpCode::Equal);
            break;
        case BinaryExpression::Operator::NotEqual:
            emit(OpCode::NotEqual);
            break;
        case BinaryExpression::Operator::StrictEqual:
            emit(OpCode::StrictEqual);
            break;
        case BinaryExpression::Operator::StrictNotEqual:
            emit(OpCode::StrictNotEqual);
            break;
        case BinaryExpression::Operator::LessThan:
            emit(OpCode::LessThan);
            break;
        case BinaryExpression::Operator::LessEqual:
            emit(OpCode::LessEqual);
            break;
        case BinaryExpression::Operator::GreaterThan:
            emit(OpCode::GreaterThan);
            break;
        case BinaryExpression::Operator::GreaterEqual:
            emit(OpCode::GreaterEqual);
            break;
        case BinaryExpression::Operator::BitwiseAnd:
            emit(OpCode::BitwiseAnd);
            break;
        case BinaryExpression::Operator::BitwiseOr:
            emit(OpCode::BitwiseOr);
            break;
        case BinaryExpression::Operator::BitwiseXor:
            emit(OpCode::BitwiseXor);
            break;
        case BinaryExpression::Operator::LeftShift:
            emit(OpCode::LeftShift);
            break;
        case BinaryExpression::Operator::RightShift:
            emit(OpCode::RightShift);
            break;
        case BinaryExpression::Operator::UnsignedRightShift:
            emit(OpCode::UnsignedRightShift);
            break;
        case BinaryExpression::Operator::Instanceof:
            emit(OpCode::Instanceof);
            break;
        case BinaryExpression::Operator::In:
            emit(OpCode::In);
            break;
    }
}

void Compiler::compile_logical_expression(const LogicalExpression& expr) {
    compile_expression(*expr.left);

    usize jump = 0;
    if (expr.op == LogicalExpression::Operator::And) {
        jump = emit_jump(OpCode::JumpIfFalseKeep);
    } else {
        jump = emit_jump(OpCode::JumpIfTrueKeep);
    }

    emit(OpCode::Pop);
    compile_expression(*expr.right);
    patch_jump(jump);
}

void Compiler::compile_conditional_expression(const ConditionalExpression& expr) {
    compile_expression(*expr.test);

    usize then_jump = emit_jump(OpCode::JumpIfFalse);
    emit(OpCode::Pop);
    compile_expression(*expr.consequent);

    usize else_jump = emit_jump(OpCode::Jump);
    patch_jump(then_jump);
    emit(OpCode::Pop);
    compile_expression(*expr.alternate);

    patch_jump(else_jump);
}

void Compiler::compile_assignment_expression(const AssignmentExpression& expr) {
    if (auto* id = dynamic_cast<const Identifier*>(expr.left.get())) {
        compile_expression(*expr.right);
        emit(OpCode::Dup);

        i32 local = resolve_local(m_current, id->name);
        if (local != -1) {
            emit(OpCode::SetLocal);
            emit(static_cast<u8>(local));
        } else {
            i32 upvalue = resolve_upvalue(m_current, id->name);
            if (upvalue != -1) {
                emit(OpCode::SetUpvalue);
                emit(static_cast<u8>(upvalue));
            } else {
                u16 idx = identifier_constant(id->name);
                emit(OpCode::SetGlobal);
                emit(static_cast<u8>(idx));
            }
        }
    } else if (auto* member = dynamic_cast<const MemberExpression*>(expr.left.get())) {
        compile_expression(*member->object);
        compile_expression(*expr.right);

        if (member->computed) {
            compile_expression(*member->property);
            emit(OpCode::SetElement);
        } else {
            if (auto* prop = dynamic_cast<const Identifier*>(member->property.get())) {
                u16 idx = identifier_constant(prop->name);
                emit(OpCode::SetProperty);
                emit(static_cast<u8>(idx));
            }
        }
    }
}

void Compiler::compile_sequence_expression(const SequenceExpression& expr) {
    for (usize i = 0; i < expr.expressions.size(); ++i) {
        compile_expression(*expr.expressions[i]);
        if (i < expr.expressions.size() - 1) {
            emit(OpCode::Pop);
        }
    }
}

// ============================================================================
// Pattern compilation
// ============================================================================

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

// ============================================================================
// Function compilation
// ============================================================================

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

    // Add implicit 'this' or function slot
    state.locals.push_back({"", 0, false, false});

    // Parameters
    for (const auto& param : params) {
        if (auto* id = dynamic_cast<const Identifier*>(param.get())) {
            declare_variable(id->name);
            mark_initialized();
        }
    }

    // Body
    if (auto* block = dynamic_cast<const BlockStatement*>(&body)) {
        for (const auto& stmt : block->body) {
            compile_statement(*stmt);
        }
    } else if (auto* expr = dynamic_cast<const Expression*>(&body)) {
        // Arrow function with expression body
        compile_expression(*expr);
        emit(OpCode::Return);
    }

    // Implicit return
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

    // Emit closure
    u16 func_idx = current_chunk().add_constant(name);
    emit(OpCode::Closure);
    emit(static_cast<u8>(func_idx));
    emit(static_cast<u8>(state.upvalues.size()));

    for (const auto& upvalue : state.upvalues) {
        emit(upvalue.is_local ? 1 : 0);
        emit(upvalue.index);
    }
}

// ============================================================================
// Error handling
// ============================================================================

void Compiler::error(const String& message) {
    if (m_panic_mode) return;
    m_panic_mode = true;
    m_had_error = true;
    m_errors.push_back(message);
    if (m_error_callback) {
        m_error_callback(message, 0, 0);
    }
}

void Compiler::error_at(const SourceLocation& loc, const String& message) {
    if (m_panic_mode) return;
    m_panic_mode = true;
    m_had_error = true;
    m_errors.push_back(message);
    if (m_error_callback) {
        m_error_callback(message, loc.line, loc.column);
    }
}

// ============================================================================
// Loop context
// ============================================================================

void Compiler::push_loop(usize start) {
    m_loop_stack.push_back({start, {}, {}, m_current->scope_depth});
}

void Compiler::pop_loop() {
    auto& loop = m_loop_stack.back();

    // Patch break jumps to current position
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

    // Pop locals up to loop scope
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

// ============================================================================
// Convenience functions
// ============================================================================

std::shared_ptr<Function> compile(const String& source) {
    Parser parser;
    parser.set_input(source);
    auto program = parser.parse_program();

    if (parser.has_errors()) {
        return nullptr;
    }

    Compiler compiler;
    return compiler.compile(*program);
}

std::shared_ptr<Function> compile(const Program& program) {
    Compiler compiler;
    return compiler.compile(program);
}

} // namespace lithium::js
