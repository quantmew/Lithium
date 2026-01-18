/**
 * JavaScript compiler: AST -> bytecode (stack VM)
 */

#include "lithium/js/compiler.hpp"
#include <optional>
#include <unordered_set>

namespace lithium::js {

namespace {

struct FunctionBuilder {
    std::shared_ptr<FunctionCode> code;
    bool is_entry{false};
};

// ============================================================================
// Escape Analysis - determines which allocations can use stack allocation
// ============================================================================

class EscapeAnalyzer {
public:
    // Analyze a function to find variables that don't escape
    void analyze_function(const std::vector<StatementPtr>& body) {
        m_escaping_vars.clear();
        m_new_expr_vars.clear();
        m_current_depth = 0;
        for (const auto& stmt : body) {
            analyze_statement(*stmt);
        }
    }

    // Check if a new expression assigned to a variable can use stack allocation
    bool can_stack_allocate(const String& var_name) const {
        // Variable must hold a new expression result and not escape
        return m_new_expr_vars.count(var_name) > 0 &&
               m_escaping_vars.count(var_name) == 0;
    }

    // Track that a variable holds a new expression result
    void mark_new_expr_var(const String& var_name) {
        if (m_current_depth == 0) {
            m_new_expr_vars.insert(var_name);
        }
    }

private:
    std::unordered_set<String> m_escaping_vars;   // Variables that escape
    std::unordered_set<String> m_new_expr_vars;   // Variables holding new expressions
    int m_current_depth{0};  // Nested function depth

    void mark_escaping(const String& var_name) {
        m_escaping_vars.insert(var_name);
    }

    void analyze_statement(const Statement& stmt) {
        if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(&stmt)) {
            analyze_expression(*expr_stmt->expression);
            return;
        }
        if (auto* block = dynamic_cast<const BlockStatement*>(&stmt)) {
            for (const auto& s : block->body) {
                analyze_statement(*s);
            }
            return;
        }
        if (auto* decl = dynamic_cast<const VariableDeclaration*>(&stmt)) {
            for (const auto& var : decl->declarations) {
                if (var.init) {
                    // Check if init is a new expression
                    if (dynamic_cast<const NewExpression*>(var.init.get())) {
                        mark_new_expr_var(var.id);
                    }
                    analyze_expression(*var.init);
                }
            }
            return;
        }
        if (auto* ret = dynamic_cast<const ReturnStatement*>(&stmt)) {
            if (ret->argument) {
                // Returned values escape
                mark_expression_escaping(*ret->argument);
                analyze_expression(*ret->argument);
            }
            return;
        }
        if (auto* if_stmt = dynamic_cast<const IfStatement*>(&stmt)) {
            analyze_expression(*if_stmt->test);
            analyze_statement(*if_stmt->consequent);
            if (if_stmt->alternate) {
                analyze_statement(*if_stmt->alternate);
            }
            return;
        }
        if (auto* while_stmt = dynamic_cast<const WhileStatement*>(&stmt)) {
            analyze_expression(*while_stmt->test);
            analyze_statement(*while_stmt->body);
            return;
        }
        if (auto* for_stmt = dynamic_cast<const ForStatement*>(&stmt)) {
            if (for_stmt->init_statement) analyze_statement(*for_stmt->init_statement);
            if (for_stmt->init_expression) analyze_expression(*for_stmt->init_expression);
            if (for_stmt->test) analyze_expression(*for_stmt->test);
            if (for_stmt->update) analyze_expression(*for_stmt->update);
            analyze_statement(*for_stmt->body);
            return;
        }
        if (auto* try_stmt = dynamic_cast<const TryStatement*>(&stmt)) {
            analyze_statement(*try_stmt->block);
            if (try_stmt->handler) {
                analyze_statement(*try_stmt->handler);
            }
            if (try_stmt->finalizer) {
                analyze_statement(*try_stmt->finalizer);
            }
            return;
        }
        if (auto* throw_stmt = dynamic_cast<const ThrowStatement*>(&stmt)) {
            mark_expression_escaping(*throw_stmt->argument);
            analyze_expression(*throw_stmt->argument);
            return;
        }
        if (auto* fn_decl = dynamic_cast<const FunctionDeclaration*>(&stmt)) {
            // Enter nested function - any variables used here escape
            m_current_depth++;
            for (const auto& s : fn_decl->body) {
                analyze_statement(*s);
            }
            m_current_depth--;
            return;
        }
    }

    void analyze_expression(const Expression& expr) {
        if (auto* call = dynamic_cast<const CallExpression*>(&expr)) {
            analyze_expression(*call->callee);
            // Arguments passed to calls escape
            for (const auto& arg : call->arguments) {
                mark_expression_escaping(*arg);
                analyze_expression(*arg);
            }
            return;
        }
        if (auto* new_expr = dynamic_cast<const NewExpression*>(&expr)) {
            analyze_expression(*new_expr->callee);
            for (const auto& arg : new_expr->arguments) {
                mark_expression_escaping(*arg);
                analyze_expression(*arg);
            }
            return;
        }
        if (auto* assign = dynamic_cast<const AssignmentExpression*>(&expr)) {
            analyze_expression(*assign->right);
            // If assigning to a property, the right side escapes
            if (dynamic_cast<const MemberExpression*>(assign->left.get())) {
                mark_expression_escaping(*assign->right);
            }
            // If assigning to a variable and we're in a nested function, it escapes
            if (m_current_depth > 0) {
                if (auto* id = dynamic_cast<const Identifier*>(assign->left.get())) {
                    mark_escaping(id->name);
                }
            }
            return;
        }
        if (auto* member = dynamic_cast<const MemberExpression*>(&expr)) {
            analyze_expression(*member->object);
            if (member->computed && member->property) {
                analyze_expression(*member->property);
            }
            return;
        }
        if (auto* fn_expr = dynamic_cast<const FunctionExpression*>(&expr)) {
            m_current_depth++;
            if (fn_expr->concise_body) {
                analyze_expression(*fn_expr->concise_body);
            } else {
                for (const auto& s : fn_expr->body) {
                    analyze_statement(*s);
                }
            }
            m_current_depth--;
            return;
        }
        if (auto* binary = dynamic_cast<const BinaryExpression*>(&expr)) {
            analyze_expression(*binary->left);
            analyze_expression(*binary->right);
            return;
        }
        if (auto* unary = dynamic_cast<const UnaryExpression*>(&expr)) {
            analyze_expression(*unary->argument);
            return;
        }
        if (auto* cond = dynamic_cast<const ConditionalExpression*>(&expr)) {
            analyze_expression(*cond->test);
            analyze_expression(*cond->consequent);
            analyze_expression(*cond->alternate);
            return;
        }
        if (auto* array = dynamic_cast<const ArrayExpression*>(&expr)) {
            for (const auto& elem : array->elements) {
                if (elem) {
                    mark_expression_escaping(*elem);
                    analyze_expression(*elem);
                }
            }
            return;
        }
        if (auto* obj = dynamic_cast<const ObjectExpression*>(&expr)) {
            for (const auto& prop : obj->properties) {
                mark_expression_escaping(*prop.value);
                analyze_expression(*prop.value);
            }
            return;
        }
        // Identifier at depth > 0 means closure over it
        if (auto* id = dynamic_cast<const Identifier*>(&expr)) {
            if (m_current_depth > 0) {
                mark_escaping(id->name);
            }
            return;
        }
    }

    void mark_expression_escaping(const Expression& expr) {
        if (auto* id = dynamic_cast<const Identifier*>(&expr)) {
            mark_escaping(id->name);
        }
    }
};

class CompilerImpl {
public:
    Compiler::Result compile(const Program& program) {
        Compiler::Result result;
        auto entry = std::make_shared<FunctionCode>();
        entry->name = "(script)"_s;
        m_functions.clear();
        m_functions.push_back(entry);

        collect_locals(*entry, program.body);
        compile_statements(program.body, *entry, /*is_entry*/true);
        emit_return(*entry);

        result.module.functions = m_functions;
        result.module.entry = 0;
        result.errors = m_errors;
        return result;
    }

private:
    struct LoopContext {
        std::vector<usize> break_patches;
        std::vector<usize> continue_patches;
        usize continue_target{0};
        bool allows_continue{true};
    };

    void collect_locals(FunctionCode& fn, const std::vector<StatementPtr>& stmts) {
        for (const auto& stmt : stmts) {
            collect_locals_in_statement(*stmt, fn);
        }
    }

    void collect_locals_in_statement(const Statement& stmt, FunctionCode& fn) {
        if (auto* block = dynamic_cast<const BlockStatement*>(&stmt)) {
            for (const auto& s : block->body) {
                collect_locals_in_statement(*s, fn);
            }
            return;
        }
        if (auto* decl = dynamic_cast<const VariableDeclaration*>(&stmt)) {
            bool is_const = decl->kind == VariableDeclaration::Kind::Const;
            for (const auto& var : decl->declarations) {
                fn.add_local(var.id, is_const);
            }
            return;
        }
        if (auto* fn_decl = dynamic_cast<const FunctionDeclaration*>(&stmt)) {
            fn.add_local(fn_decl->name, true);
            return;
        }
        if (auto* if_stmt = dynamic_cast<const IfStatement*>(&stmt)) {
            collect_locals_in_statement(*if_stmt->consequent, fn);
            if (if_stmt->alternate) {
                collect_locals_in_statement(*if_stmt->alternate, fn);
            }
            return;
        }
        if (auto* while_stmt = dynamic_cast<const WhileStatement*>(&stmt)) {
            collect_locals_in_statement(*while_stmt->body, fn);
            return;
        }
        if (auto* do_while = dynamic_cast<const DoWhileStatement*>(&stmt)) {
            collect_locals_in_statement(*do_while->body, fn);
            return;
        }
        if (auto* for_stmt = dynamic_cast<const ForStatement*>(&stmt)) {
            if (for_stmt->init_statement) collect_locals_in_statement(*for_stmt->init_statement, fn);
            if (for_stmt->body) collect_locals_in_statement(*for_stmt->body, fn);
            return;
        }
        if (auto* switch_stmt = dynamic_cast<const SwitchStatement*>(&stmt)) {
            for (const auto& case_stmt : switch_stmt->cases) {
                for (const auto& cons : case_stmt.consequent) {
                    collect_locals_in_statement(*cons, fn);
                }
            }
            return;
        }
        if (auto* try_stmt = dynamic_cast<const TryStatement*>(&stmt)) {
            collect_locals_in_statement(*try_stmt->block, fn);
            if (try_stmt->handler) {
                if (!try_stmt->handler_param.empty()) {
                    fn.add_local(try_stmt->handler_param, false);
                }
                collect_locals_in_statement(*try_stmt->handler, fn);
            }
            if (try_stmt->finalizer) {
                collect_locals_in_statement(*try_stmt->finalizer, fn);
            }
            return;
        }
        if (auto* with_stmt = dynamic_cast<const WithStatement*>(&stmt)) {
            collect_locals_in_statement(*with_stmt->body, fn);
            return;
        }
        if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(&stmt)) {
            // Expressions don't introduce locals for the outer function in this compiler
            (void)expr_stmt;
            return;
        }
    }

    // Helpers to emit
    u16 add_constant(FunctionCode& fn, const Value& value) {
        return fn.chunk.add_constant(value);
    }

    u16 add_name(FunctionCode& fn, const String& name) {
        return add_constant(fn, Value(name));
    }

    void emit(FunctionCode& fn, OpCode op) {
        fn.chunk.write(op);
    }

    void emit_u8(FunctionCode& fn, u8 v) {
        fn.chunk.write_u8(v);
    }

    void emit_u16(FunctionCode& fn, u16 v) {
        fn.chunk.write_u16(v);
    }

    // Emit GetProp with inline cache
    void emit_get_prop_ic(FunctionCode& fn, const String& name) {
        u16 name_idx = add_name(fn, name);
        u16 cache_slot = fn.alloc_ic_slot();
        emit(fn, OpCode::GetPropIC);
        emit_u16(fn, name_idx);
        emit_u16(fn, cache_slot);
    }

    // Emit SetProp with inline cache
    void emit_set_prop_ic(FunctionCode& fn, const String& name) {
        u16 name_idx = add_name(fn, name);
        u16 cache_slot = fn.alloc_ic_slot();
        emit(fn, OpCode::SetPropIC);
        emit_u16(fn, name_idx);
        emit_u16(fn, cache_slot);
    }

    void emit_constant(FunctionCode& fn, const Value& v) {
        u16 idx = add_constant(fn, v);
        emit(fn, OpCode::LoadConst);
        emit_u16(fn, idx);
    }

    usize emit_jump(FunctionCode& fn, OpCode op) {
        emit(fn, op);
        usize operand_offset = fn.chunk.size();
        fn.chunk.write_u16(0xFFFF);
        return operand_offset;
    }

    void patch_jump(FunctionCode& fn, usize operand_offset) {
        i16 offset = static_cast<i16>(fn.chunk.size() - operand_offset - 2);
        fn.chunk.patch_i16(operand_offset, offset);
    }

    void patch_to(FunctionCode& fn, usize operand_offset, usize target) {
        i16 offset = static_cast<i16>(target) - static_cast<i16>(operand_offset + 2);
        fn.chunk.patch_i16(operand_offset, offset);
    }

    void emit_return(FunctionCode& fn) {
        emit(fn, OpCode::LoadUndefined);
        emit(fn, OpCode::Return);
    }

    // Compilation entry points
    void compile_statements(const std::vector<StatementPtr>& stmts, FunctionCode& fn, bool is_entry) {
        for (usize i = 0; i < stmts.size(); ++i) {
            bool is_last = is_entry && (i == stmts.size() - 1);
            compile_statement(*stmts[i], fn, is_last);
        }
    }

    void compile_statement(const Statement& stmt, FunctionCode& fn, bool is_entry_last) {
        if (auto* expr = dynamic_cast<const ExpressionStatement*>(&stmt)) {
            compile_expression(*expr->expression, fn);
            emit(fn, is_entry_last ? OpCode::Return : OpCode::Pop);
            return;
        }

        if (auto* block = dynamic_cast<const BlockStatement*>(&stmt)) {
            compile_statements(block->body, fn, false);
            return;
        }

        if (auto* decl = dynamic_cast<const VariableDeclaration*>(&stmt)) {
            for (const auto& var : decl->declarations) {
                auto local_slot = fn.resolve_local(var.id);
                u16 name_idx = 0;
                if (!local_slot) {
                    name_idx = add_name(fn, var.id);
                    emit(fn, OpCode::DefineVar);
                    emit_u16(fn, name_idx);
                    emit_u8(fn, decl->kind == VariableDeclaration::Kind::Const ? 1 : 0);
                }
                if (local_slot) {
                    emit(fn, OpCode::LoadUndefined);
                    emit(fn, OpCode::SetLocal);
                    emit_u16(fn, *local_slot);
                }
                if (var.init) {
                    // Check if this is a new expression that can use stack allocation
                    if (auto* new_expr = dynamic_cast<const NewExpression*>(var.init.get())) {
                        if (m_escape_analyzer.can_stack_allocate(var.id)) {
                            m_use_stack_alloc = true;
                        }
                    }
                    compile_expression(*var.init, fn);
                    m_use_stack_alloc = false;
                    if (local_slot) {
                        emit(fn, OpCode::SetLocal);
                        emit_u16(fn, *local_slot);
                    } else {
                        emit(fn, OpCode::SetVar);
                        emit_u16(fn, name_idx);
                    }
                }
            }
            return;
        }

        if (auto* fn_decl = dynamic_cast<const FunctionDeclaration*>(&stmt)) {
            u16 func_idx = compile_function(fn_decl->name, fn_decl->params, fn_decl->body, nullptr);
            auto local_slot = fn.resolve_local(fn_decl->name);
            u16 name_idx = local_slot ? 0 : add_name(fn, fn_decl->name);
            emit(fn, OpCode::MakeFunction);
            emit_u16(fn, func_idx);
            if (local_slot) {
                emit(fn, OpCode::SetLocal);
                emit_u16(fn, *local_slot);
            } else {
                emit(fn, OpCode::DefineVar);
                emit_u16(fn, name_idx);
                emit_u8(fn, 1); // const binding
                emit(fn, OpCode::SetVar);
                emit_u16(fn, name_idx);
            }
            return;
        }

        if (auto* ret = dynamic_cast<const ReturnStatement*>(&stmt)) {
            if (ret->argument) {
                compile_expression(*ret->argument, fn);
            } else {
                emit(fn, OpCode::LoadUndefined);
            }
            emit(fn, OpCode::Return);
            return;
        }

        if (auto* br = dynamic_cast<const BreakStatement*>(&stmt)) {
            if (m_loop_stack.empty()) {
                m_errors.push_back("break used outside of loop or switch"_s);
            } else {
                usize patch = emit_jump(fn, OpCode::Jump);
                m_loop_stack.back().break_patches.push_back(patch);
            }
            emit(fn, is_entry_last ? OpCode::Return : OpCode::Pop);
            return;
        }

        if (auto* cont = dynamic_cast<const ContinueStatement*>(&stmt)) {
            if (m_loop_stack.empty() || !m_loop_stack.back().allows_continue) {
                m_errors.push_back("continue used outside of loop"_s);
            } else {
                usize patch = emit_jump(fn, OpCode::Jump);
                m_loop_stack.back().continue_patches.push_back(patch);
            }
            emit(fn, is_entry_last ? OpCode::Return : OpCode::Pop);
            return;
        }

        if (auto* if_stmt = dynamic_cast<const IfStatement*>(&stmt)) {
            compile_expression(*if_stmt->test, fn);
            usize else_jump = emit_jump(fn, OpCode::JumpIfFalse);
            emit(fn, OpCode::Pop);
            compile_statement(*if_stmt->consequent, fn, false);
            usize end_jump = emit_jump(fn, OpCode::Jump);
            patch_jump(fn, else_jump);
            emit(fn, OpCode::Pop);
            if (if_stmt->alternate) {
                compile_statement(*if_stmt->alternate, fn, false);
            }
            patch_jump(fn, end_jump);
            return;
        }

        if (auto* while_stmt = dynamic_cast<const WhileStatement*>(&stmt)) {
            usize loop_start = fn.chunk.size();
            compile_expression(*while_stmt->test, fn);
            usize exit_jump = emit_jump(fn, OpCode::JumpIfFalse);
            emit(fn, OpCode::Pop);

            LoopContext loop;
            loop.continue_target = loop_start;
            m_loop_stack.push_back(loop);
            compile_statement(*while_stmt->body, fn, false);
            for (usize patch : m_loop_stack.back().continue_patches) {
                patch_to(fn, patch, loop_start);
            }
            emit(fn, OpCode::Jump);
            usize back_operand = fn.chunk.size();
            fn.chunk.write_i16(0);
            i16 offset = static_cast<i16>(loop_start) - static_cast<i16>(back_operand + 2);
            fn.chunk.patch_i16(back_operand, offset);
            patch_jump(fn, exit_jump);
            for (usize patch : m_loop_stack.back().break_patches) {
                patch_jump(fn, patch);
            }
            m_loop_stack.pop_back();
            emit(fn, OpCode::Pop);
            return;
        }

        if (auto* do_while = dynamic_cast<const DoWhileStatement*>(&stmt)) {
            usize loop_start = fn.chunk.size();
            LoopContext loop;
            loop.continue_target = 0;
            m_loop_stack.push_back(loop);
            compile_statement(*do_while->body, fn, false);
            usize test_start = fn.chunk.size();
            m_loop_stack.back().continue_target = test_start;
            for (usize patch : m_loop_stack.back().continue_patches) {
                patch_to(fn, patch, test_start);
            }
            compile_expression(*do_while->test, fn);
            usize exit_jump = emit_jump(fn, OpCode::JumpIfFalse);
            emit(fn, OpCode::Pop);
            emit(fn, OpCode::Jump);
            usize back_operand = fn.chunk.size();
            fn.chunk.write_i16(0);
            i16 offset = static_cast<i16>(loop_start) - static_cast<i16>(back_operand + 2);
            fn.chunk.patch_i16(back_operand, offset);
            patch_jump(fn, exit_jump);
            for (usize patch : m_loop_stack.back().break_patches) {
                patch_jump(fn, patch);
            }
            m_loop_stack.pop_back();
            emit(fn, OpCode::Pop);
            return;
        }

        if (auto* for_stmt = dynamic_cast<const ForStatement*>(&stmt)) {
            if (for_stmt->init_statement) {
                compile_statement(*for_stmt->init_statement, fn, false);
            } else if (for_stmt->init_expression) {
                compile_expression(*for_stmt->init_expression, fn);
                emit(fn, OpCode::Pop);
            }

            usize loop_start = fn.chunk.size();
            usize exit_jump = 0;
            if (for_stmt->test) {
                compile_expression(*for_stmt->test, fn);
                exit_jump = emit_jump(fn, OpCode::JumpIfFalse);
                emit(fn, OpCode::Pop);
            }

            LoopContext loop;
            loop.continue_target = 0;
            m_loop_stack.push_back(loop);
            compile_statement(*for_stmt->body, fn, false);

            usize update_start = fn.chunk.size();
            m_loop_stack.back().continue_target = update_start;
            for (usize patch : m_loop_stack.back().continue_patches) {
                patch_to(fn, patch, update_start);
            }

            if (for_stmt->update) {
                compile_expression(*for_stmt->update, fn);
                emit(fn, OpCode::Pop);
            }

            emit(fn, OpCode::Jump);
            usize back_operand = fn.chunk.size();
            fn.chunk.write_i16(0);
            i16 offset = static_cast<i16>(loop_start) - static_cast<i16>(back_operand + 2);
            fn.chunk.patch_i16(back_operand, offset);

            if (for_stmt->test) {
                patch_jump(fn, exit_jump);
            }
            for (usize patch : m_loop_stack.back().break_patches) {
                patch_jump(fn, patch);
            }
            m_loop_stack.pop_back();
            return;
        }

        if (auto* switch_stmt = dynamic_cast<const SwitchStatement*>(&stmt)) {
            compile_expression(*switch_stmt->discriminant, fn);
            LoopContext switch_ctx;
            switch_ctx.allows_continue = false;
            m_loop_stack.push_back(switch_ctx);

            std::vector<usize> match_jumps(switch_stmt->cases.size(), 0);

            // Emit comparisons
            for (usize i = 0; i < switch_stmt->cases.size(); ++i) {
                const auto& case_entry = switch_stmt->cases[i];
                if (!case_entry.test) {
                    continue;
                }

                emit(fn, OpCode::Dup); // keep discriminant for later cases
                compile_expression(*case_entry.test, fn);
                emit(fn, OpCode::StrictEqual);
                usize fail = emit_jump(fn, OpCode::JumpIfFalse);
                emit(fn, OpCode::Pop); // discard comparison result on true path
                match_jumps[i] = emit_jump(fn, OpCode::Jump);

                usize fail_target = fn.chunk.size();
                emit(fn, OpCode::Pop); // discard comparison result on false path
                patch_to(fn, fail, fail_target);
            }

            usize no_match_jump = emit_jump(fn, OpCode::Jump);

            // Emit bodies sequentially to allow fallthrough
            usize default_start = 0;
            for (usize i = 0; i < switch_stmt->cases.size(); ++i) {
                const auto& case_entry = switch_stmt->cases[i];
                usize body_start = fn.chunk.size();
                if (case_entry.test) {
                    patch_to(fn, match_jumps[i], body_start);
                } else {
                    default_start = body_start;
                }

                for (const auto& st : case_entry.consequent) {
                    compile_statement(*st, fn, false);
                }
            }

            usize end_switch = fn.chunk.size();
            patch_to(fn, no_match_jump, default_start ? default_start : end_switch);

            for (usize patch : m_loop_stack.back().break_patches) {
                patch_jump(fn, patch);
            }
            m_loop_stack.pop_back();
            emit(fn, OpCode::Pop); // discriminant
            return;
        }

        if (auto* throw_stmt = dynamic_cast<const ThrowStatement*>(&stmt)) {
            compile_expression(*throw_stmt->argument, fn);
            emit(fn, OpCode::Throw);
            return;
        }

        if (auto* try_stmt = dynamic_cast<const TryStatement*>(&stmt)) {
            usize handler_pos = fn.chunk.size();
            emit(fn, OpCode::PushHandler);
            fn.chunk.write_u16(0); // catch ip placeholder
            fn.chunk.write_u16(0); // finally ip placeholder
            fn.chunk.write_u8(try_stmt->handler ? 1 : 0);

            compile_statement(*try_stmt->block, fn, false);
            emit(fn, OpCode::PopHandler);
            std::vector<usize> after_try_jumps;
            after_try_jumps.push_back(emit_jump(fn, OpCode::Jump));

            usize catch_ip = 0;
            if (try_stmt->handler) {
                catch_ip = fn.chunk.size();
                fn.chunk.patch_i16(handler_pos + 1, static_cast<i16>(catch_ip));
                if (!try_stmt->handler_param.empty()) {
                    auto local_slot = fn.resolve_local(try_stmt->handler_param);
                    if (local_slot) {
                        emit(fn, OpCode::SetLocal);
                        emit_u16(fn, *local_slot);
                    } else {
                        auto name_idx = add_name(fn, try_stmt->handler_param);
                        emit(fn, OpCode::DefineVar);
                        emit_u16(fn, name_idx);
                        emit_u8(fn, 0);
                        emit(fn, OpCode::SetVar);
                        emit_u16(fn, name_idx);
                    }
                }
                compile_statement(*try_stmt->handler, fn, false);
                after_try_jumps.push_back(emit_jump(fn, OpCode::Jump));
            }

            usize finally_ip = 0;
            if (try_stmt->finalizer) {
                finally_ip = fn.chunk.size();
                fn.chunk.patch_i16(handler_pos + 3, static_cast<i16>(finally_ip));
                compile_statement(*try_stmt->finalizer, fn, false);
            }

            usize target = try_stmt->finalizer ? finally_ip : fn.chunk.size();
            for (usize jump_offset : after_try_jumps) {
                patch_to(fn, jump_offset, target);
            }
            if (!catch_ip && try_stmt->finalizer) {
                fn.chunk.patch_i16(handler_pos + 1, static_cast<i16>(finally_ip));
            }
            return;
        }

        if (auto* with_stmt = dynamic_cast<const WithStatement*>(&stmt)) {
            compile_expression(*with_stmt->object, fn);
            emit(fn, OpCode::EnterWith);
            compile_statement(*with_stmt->body, fn, false);
            emit(fn, OpCode::ExitWith);
            return;
        }

        // Unknown statement type: ignore
        emit(fn, OpCode::LoadUndefined);
        emit(fn, is_entry_last ? OpCode::Return : OpCode::Pop);
    }

    // Expressions
    void compile_expression(const Expression& expr, FunctionCode& fn) {
        if (dynamic_cast<const NullLiteral*>(&expr)) {
            emit(fn, OpCode::LoadNull);
            return;
        }
        if (auto* b = dynamic_cast<const BooleanLiteral*>(&expr)) {
            emit(fn, b->value ? OpCode::LoadTrue : OpCode::LoadFalse);
            return;
        }
        if (auto* n = dynamic_cast<const NumericLiteral*>(&expr)) {
            emit_constant(fn, Value(n->value));
            return;
        }
        if (auto* s = dynamic_cast<const StringLiteral*>(&expr)) {
            emit_constant(fn, Value(s->value));
            return;
        }
        if (auto* r = dynamic_cast<const RegExpLiteral*>(&expr)) {
            String literal = "/"_s + r->pattern + "/"_s + r->flags;
            emit_constant(fn, Value(literal));
            return;
        }
        if (dynamic_cast<const ThisExpression*>(&expr)) {
            emit(fn, OpCode::This);
            return;
        }
        if (auto* id = dynamic_cast<const Identifier*>(&expr)) {
            if (auto slot = fn.resolve_local(id->name)) {
                emit(fn, OpCode::GetLocal);
                emit_u16(fn, *slot);
            } else {
                u16 name_idx = add_name(fn, id->name);
                emit(fn, OpCode::GetVar);
                emit_u16(fn, name_idx);
            }
            return;
        }
        if (auto* tpl = dynamic_cast<const TemplateLiteral*>(&expr)) {
            if (tpl->quasis.empty()) {
                emit_constant(fn, Value(""_s));
                return;
            }
            emit_constant(fn, Value(tpl->quasis.front().value));
            for (usize i = 0; i < tpl->expressions.size(); ++i) {
                compile_expression(*tpl->expressions[i], fn);
                emit(fn, OpCode::Add);
                emit_constant(fn, Value(tpl->quasis[i + 1].value));
                emit(fn, OpCode::Add);
            }
            return;
        }
        if (auto* arr = dynamic_cast<const ArrayExpression*>(&expr)) {
            emit(fn, OpCode::MakeArray);
            emit_u16(fn, 0);
            for (const auto& el : arr->elements) {
                if (auto* spread = dynamic_cast<SpreadElement*>(el.get())) {
                    compile_expression(*spread->argument, fn);
                    emit(fn, OpCode::ArraySpread);
                } else {
                    compile_expression(*el, fn);
                    emit(fn, OpCode::ArrayPush);
                }
            }
            return;
        }
        if (auto* obj = dynamic_cast<const ObjectExpression*>(&expr)) {
            emit(fn, OpCode::MakeObject);
            for (const auto& prop : obj->properties) {
                if (prop.spread) {
                    compile_expression(*prop.value, fn);
                    emit(fn, OpCode::ObjectSpread);
                    continue;
                }
                emit(fn, OpCode::Dup);
                compile_expression(*prop.value, fn);
                if (prop.computed) {
                    compile_expression(*prop.computed_key, fn);
                    emit(fn, OpCode::SetElem);
                } else {
                    // Use IC-enabled property access
                    emit_set_prop_ic(fn, prop.key);
                }
                emit(fn, OpCode::Pop); // discard value
            }
            return;
        }
        if (auto* member = dynamic_cast<const MemberExpression*>(&expr)) {
            compile_expression(*member->object, fn);
            if (member->optional) {
                usize nullish = emit_jump(fn, OpCode::JumpIfNullish);
                if (member->computed) {
                    compile_expression(*member->property, fn);
                    emit(fn, OpCode::GetElem);
                } else if (auto* pid = dynamic_cast<Identifier*>(member->property.get())) {
                    // Use IC-enabled property access
                    emit_get_prop_ic(fn, pid->name);
                }
                usize end = emit_jump(fn, OpCode::Jump);
                patch_jump(fn, nullish);
                emit(fn, OpCode::Pop);
                emit(fn, OpCode::LoadUndefined);
                patch_jump(fn, end);
            } else {
                if (member->computed) {
                    compile_expression(*member->property, fn);
                    emit(fn, OpCode::GetElem);
                } else if (auto* pid = dynamic_cast<Identifier*>(member->property.get())) {
                    // Use IC-enabled property access
                    emit_get_prop_ic(fn, pid->name);
                }
            }
            return;
        }
        if (auto* call = dynamic_cast<const CallExpression*>(&expr)) {
            compile_expression(*call->callee, fn);
            if (call->optional) {
                usize nullish = emit_jump(fn, OpCode::JumpIfNullish);
                for (const auto& arg : call->arguments) {
                    compile_expression(*arg, fn);
                }
                emit(fn, OpCode::Call);
                emit_u16(fn, static_cast<u16>(call->arguments.size()));
                usize end = emit_jump(fn, OpCode::Jump);
                patch_jump(fn, nullish);
                emit(fn, OpCode::Pop);
                emit(fn, OpCode::LoadUndefined);
                patch_jump(fn, end);
            } else {
                for (const auto& arg : call->arguments) {
                    compile_expression(*arg, fn);
                }
                emit(fn, OpCode::Call);
                emit_u16(fn, static_cast<u16>(call->arguments.size()));
            }
            return;
        }
        if (auto* new_expr = dynamic_cast<const NewExpression*>(&expr)) {
            compile_expression(*new_expr->callee, fn);
            for (const auto& arg : new_expr->arguments) {
                compile_expression(*arg, fn);
            }
            // Use stack allocation if escape analysis determined it's safe
            emit(fn, m_use_stack_alloc ? OpCode::NewStack : OpCode::New);
            emit_u16(fn, static_cast<u16>(new_expr->arguments.size()));
            return;
        }
        if (auto* unary = dynamic_cast<const UnaryExpression*>(&expr)) {
            switch (unary->op) {
                case UnaryExpression::Operator::Minus:
                    compile_expression(*unary->argument, fn);
                    emit(fn, OpCode::Negate);
                    break;
                case UnaryExpression::Operator::Plus:
                    compile_expression(*unary->argument, fn);
                    break;
                case UnaryExpression::Operator::Not:
                    compile_expression(*unary->argument, fn);
                    emit(fn, OpCode::LogicalNot);
                    break;
                case UnaryExpression::Operator::Typeof:
                    compile_expression(*unary->argument, fn);
                    emit(fn, OpCode::Typeof);
                    break;
                case UnaryExpression::Operator::Void:
                    compile_expression(*unary->argument, fn);
                    emit(fn, OpCode::Pop);
                    emit(fn, OpCode::Void);
                    break;
                case UnaryExpression::Operator::Delete:
                    compile_expression(*unary->argument, fn);
                    emit(fn, OpCode::Pop);
                    emit(fn, OpCode::LoadTrue);
                    break;
                case UnaryExpression::Operator::Await:
                    compile_expression(*unary->argument, fn);
                    break;
                case UnaryExpression::Operator::BitwiseNot:
                    compile_expression(*unary->argument, fn);
                    emit(fn, OpCode::BitwiseNot);
                    break;
            }
            return;
        }
        if (auto* update = dynamic_cast<const UpdateExpression*>(&expr)) {
            if (auto* id = dynamic_cast<Identifier*>(update->argument.get())) {
                auto local_slot = fn.resolve_local(id->name);
                u16 name_idx = local_slot ? 0 : add_name(fn, id->name);
                if (local_slot) {
                    emit(fn, OpCode::GetLocal);
                    emit_u16(fn, *local_slot);
                } else {
                    emit(fn, OpCode::GetVar);
                    emit_u16(fn, name_idx);
                }
                if (!update->prefix) {
                    emit(fn, OpCode::Dup);
                }
                emit_constant(fn, Value(1.0));
                emit(fn, update->op == UpdateExpression::Operator::Increment ? OpCode::Add : OpCode::Subtract);
                if (local_slot) {
                    emit(fn, OpCode::SetLocal);
                    emit_u16(fn, *local_slot);
                } else {
                    emit(fn, OpCode::SetVar);
                    emit_u16(fn, name_idx);
                }
                if (!update->prefix) {
                    emit(fn, OpCode::Pop);
                }
            } else {
                m_errors.push_back("Unsupported update target"_s);
                emit(fn, OpCode::LoadUndefined);
            }
            return;
        }
        if (auto* binary = dynamic_cast<const BinaryExpression*>(&expr)) {
            compile_expression(*binary->left, fn);
            compile_expression(*binary->right, fn);
            switch (binary->op) {
                case BinaryExpression::Operator::Add: emit(fn, OpCode::Add); break;
                case BinaryExpression::Operator::Subtract: emit(fn, OpCode::Subtract); break;
                case BinaryExpression::Operator::Multiply: emit(fn, OpCode::Multiply); break;
                case BinaryExpression::Operator::Divide: emit(fn, OpCode::Divide); break;
                case BinaryExpression::Operator::Modulo: emit(fn, OpCode::Modulo); break;
                case BinaryExpression::Operator::Exponent: emit(fn, OpCode::Exponent); break;
                case BinaryExpression::Operator::LeftShift: emit(fn, OpCode::LeftShift); break;
                case BinaryExpression::Operator::RightShift: emit(fn, OpCode::RightShift); break;
                case BinaryExpression::Operator::UnsignedRightShift: emit(fn, OpCode::UnsignedRightShift); break;
                case BinaryExpression::Operator::BitwiseAnd: emit(fn, OpCode::BitwiseAnd); break;
                case BinaryExpression::Operator::BitwiseOr: emit(fn, OpCode::BitwiseOr); break;
                case BinaryExpression::Operator::BitwiseXor: emit(fn, OpCode::BitwiseXor); break;
                case BinaryExpression::Operator::StrictEqual: emit(fn, OpCode::StrictEqual); break;
                case BinaryExpression::Operator::StrictNotEqual: emit(fn, OpCode::StrictNotEqual); break;
                case BinaryExpression::Operator::LessThan: emit(fn, OpCode::LessThan); break;
                case BinaryExpression::Operator::LessEqual: emit(fn, OpCode::LessEqual); break;
                case BinaryExpression::Operator::GreaterThan: emit(fn, OpCode::GreaterThan); break;
                case BinaryExpression::Operator::GreaterEqual: emit(fn, OpCode::GreaterEqual); break;
                case BinaryExpression::Operator::Equal:
                case BinaryExpression::Operator::NotEqual:
                    emit(fn, binary->op == BinaryExpression::Operator::Equal ? OpCode::StrictEqual : OpCode::StrictNotEqual);
                    break;
                case BinaryExpression::Operator::Instanceof: emit(fn, OpCode::Instanceof); break;
                case BinaryExpression::Operator::In: emit(fn, OpCode::In); break;
            }
            return;
        }
        if (auto* logical = dynamic_cast<const LogicalExpression*>(&expr)) {
            compile_expression(*logical->left, fn);
            if (logical->op == LogicalExpression::Operator::Or) {
                usize to_right = emit_jump(fn, OpCode::JumpIfFalse);
                usize end_jump = emit_jump(fn, OpCode::Jump);
                patch_jump(fn, to_right);
                emit(fn, OpCode::Pop);
                compile_expression(*logical->right, fn);
                patch_jump(fn, end_jump);
            } else if (logical->op == LogicalExpression::Operator::And) {
                usize end_jump = emit_jump(fn, OpCode::JumpIfFalse);
                emit(fn, OpCode::Pop);
                compile_expression(*logical->right, fn);
                patch_jump(fn, end_jump);
            } else { // Nullish
                usize to_right = emit_jump(fn, OpCode::JumpIfNullish);
                usize end_jump = emit_jump(fn, OpCode::Jump);
                patch_jump(fn, to_right);
                emit(fn, OpCode::Pop);
                compile_expression(*logical->right, fn);
                patch_jump(fn, end_jump);
            }
            return;
        }
        if (auto* conditional = dynamic_cast<const ConditionalExpression*>(&expr)) {
            compile_expression(*conditional->test, fn);
            usize else_jump = emit_jump(fn, OpCode::JumpIfFalse);
            emit(fn, OpCode::Pop);
            compile_expression(*conditional->consequent, fn);
            usize end_jump = emit_jump(fn, OpCode::Jump);
            patch_jump(fn, else_jump);
            emit(fn, OpCode::Pop);
            compile_expression(*conditional->alternate, fn);
            patch_jump(fn, end_jump);
            return;
        }
        if (auto* assign = dynamic_cast<const AssignmentExpression*>(&expr)) {
            compile_assignment(*assign, fn);
            return;
        }
        if (auto* fn_expr = dynamic_cast<const FunctionExpression*>(&expr)) {
            u16 func_idx = compile_function(fn_expr->name.value_or(""_s), fn_expr->params, fn_expr->body,
                                           fn_expr->expression_body ? fn_expr->concise_body.get() : nullptr);
            emit(fn, OpCode::MakeFunction);
            emit_u16(fn, func_idx);
            return;
        }

        // Fallback
        emit(fn, OpCode::LoadUndefined);
    }

    void compile_assignment(const AssignmentExpression& expr, FunctionCode& fn) {
        using Op = AssignmentExpression::Operator;
        bool is_compound = expr.op != Op::Assign;

        if (auto* id = dynamic_cast<Identifier*>(expr.left.get())) {
            auto local_slot = fn.resolve_local(id->name);
            u16 name_idx = local_slot ? 0 : add_name(fn, id->name);
            if (is_compound) {
                // Load current value first
                if (local_slot) {
                    emit(fn, OpCode::GetLocal);
                    emit_u16(fn, *local_slot);
                } else {
                    emit(fn, OpCode::GetVar);
                    emit_u16(fn, name_idx);
                }
            }
            compile_expression(*expr.right, fn);
            if (is_compound) {
                emit_compound_op(expr.op, fn);
            }
            if (local_slot) {
                emit(fn, OpCode::SetLocal);
                emit_u16(fn, *local_slot);
            } else {
                emit(fn, OpCode::SetVar);
                emit_u16(fn, name_idx);
            }
            return;
        }

        if (auto* member = dynamic_cast<MemberExpression*>(expr.left.get())) {
            compile_expression(*member->object, fn);
            if (member->computed) {
                compile_expression(*member->property, fn);
            }

            if (is_compound) {
                // Duplicate object (and key if computed) for later SetProp/SetElem
                if (member->computed) {
                    emit(fn, OpCode::Dup2);  // Dup [obj, key]
                    emit(fn, OpCode::GetElem);
                } else if (auto* pid = dynamic_cast<Identifier*>(member->property.get())) {
                    emit(fn, OpCode::Dup);   // Dup obj
                    // Use IC-enabled property access
                    emit_get_prop_ic(fn, pid->name);
                }
            }

            compile_expression(*expr.right, fn);

            if (is_compound) {
                emit_compound_op(expr.op, fn);
            }

            if (member->computed) {
                emit(fn, OpCode::SetElem);
            } else if (auto* pid = dynamic_cast<Identifier*>(member->property.get())) {
                // Use IC-enabled property access
                emit_set_prop_ic(fn, pid->name);
            }
            return;
        }
    }

    void emit_compound_op(AssignmentExpression::Operator op, FunctionCode& fn) {
        using Op = AssignmentExpression::Operator;
        switch (op) {
            case Op::AddAssign: emit(fn, OpCode::Add); break;
            case Op::SubtractAssign: emit(fn, OpCode::Subtract); break;
            case Op::MultiplyAssign: emit(fn, OpCode::Multiply); break;
            case Op::DivideAssign: emit(fn, OpCode::Divide); break;
            case Op::ModuloAssign: emit(fn, OpCode::Modulo); break;
            case Op::ExponentAssign: emit(fn, OpCode::Exponent); break;
            case Op::LeftShiftAssign: emit(fn, OpCode::LeftShift); break;
            case Op::RightShiftAssign: emit(fn, OpCode::RightShift); break;
            case Op::UnsignedRightShiftAssign: emit(fn, OpCode::UnsignedRightShift); break;
            case Op::BitwiseAndAssign: emit(fn, OpCode::BitwiseAnd); break;
            case Op::BitwiseOrAssign: emit(fn, OpCode::BitwiseOr); break;
            case Op::BitwiseXorAssign: emit(fn, OpCode::BitwiseXor); break;
            default: break;
        }
    }

    u16 compile_function(const String& name,
                         const std::vector<String>& params,
                         const std::vector<StatementPtr>& body,
                         const Expression* concise_body) {
        auto fn = std::make_shared<FunctionCode>();
        fn->name = name;
        fn->params = params;
        for (const auto& param : params) {
            fn->add_local(param, false);
        }
        if (!concise_body) {
            collect_locals(*fn, body);
        }

        // Run escape analysis before compilation
        EscapeAnalyzer saved_analyzer = m_escape_analyzer;
        if (!concise_body) {
            m_escape_analyzer.analyze_function(body);
        }

        if (concise_body) {
            compile_expression(*concise_body, *fn);
            emit(*fn, OpCode::Return);
        } else {
            compile_statements(body, *fn, false);
            emit(*fn, OpCode::LoadUndefined);
            emit(*fn, OpCode::Return);
        }

        // Restore analyzer for outer function
        m_escape_analyzer = saved_analyzer;

        m_functions.push_back(fn);
        return static_cast<u16>(m_functions.size() - 1);
    }

    std::vector<std::shared_ptr<FunctionCode>> m_functions;
    std::vector<String> m_errors;
    std::vector<LoopContext> m_loop_stack;
    EscapeAnalyzer m_escape_analyzer;
    bool m_use_stack_alloc{false};  // Flag for stack-allocating the next new expression
};

} // namespace

Compiler::Result Compiler::compile(const Program& program) {
    CompilerImpl impl;
    return impl.compile(program);
}

} // namespace lithium::js
