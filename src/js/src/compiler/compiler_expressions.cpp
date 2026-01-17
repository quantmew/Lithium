/**
 * JavaScript Compiler - expression compilation
 */

#include "lithium/js/compiler.hpp"

namespace lithium::js {

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
            if (!first) {
                emit(OpCode::Add);
            }
            first = false;
        }
    }

    if (first) {
        emit_constant(""_s);
    }
}

void Compiler::compile_identifier(const Identifier& id) {
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
            compile_expression(**elem);
        } else {
            emit(OpCode::LoadUndefined);
        }
    }
    emit(OpCode::CreateArray);
    emit(static_cast<u8>(expr.elements.size()));
}

void Compiler::compile_object_expression(const ObjectExpression& expr) {
    emit(OpCode::CreateObject);
    emit(static_cast<u8>(expr.properties.size()));
    for (const auto& prop : expr.properties) {
        if (auto* id = dynamic_cast<const Identifier*>(prop.key.get())) {
            emit_constant(id->name);
        } else if (auto* lit = dynamic_cast<const StringLiteral*>(prop.key.get())) {
            emit_constant(lit->value);
        }
        compile_expression(*prop.value);
    }
}

void Compiler::compile_function_expression(const FunctionExpression& expr) {
    compile_function_body(expr.id ? expr.id->name : ""_s, expr.params, *expr.body,
                          CompilerState::Type::Function);
}

void Compiler::compile_arrow_function(const ArrowFunctionExpression& expr) {
    compile_function_body(""_s, expr.params, *expr.body, CompilerState::Type::Function);
}

void Compiler::compile_class_expression(const ClassExpression& expr) {
    emit(OpCode::Class);
    emit(0);

    begin_scope();
    add_local("this"_s);
    mark_initialized();

    for (const auto& method : expr.body) {
        if (auto* func = dynamic_cast<const FunctionExpression*>(method.get())) {
            compile_function_body(func->id ? func->id->name : ""_s, func->params, *func->body,
                                  func->kind == FunctionExpression::Kind::Constructor
                                      ? CompilerState::Type::Initializer
                                      : CompilerState::Type::Method);
        }
    }

    end_scope();
}

void Compiler::compile_member_expression(const MemberExpression& expr) {
    compile_expression(*expr.object);
    if (expr.computed) {
        compile_expression(*expr.property);
        emit(OpCode::GetElement);
    } else {
        if (auto* id = dynamic_cast<const Identifier*>(expr.property.get())) {
            u16 idx = identifier_constant(id->name);
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
    emit(OpCode::New);
    emit(static_cast<u8>(expr.arguments.size()));
}

void Compiler::compile_unary_expression(const UnaryExpression& expr) {
    compile_expression(*expr.argument);

    switch (expr.op) {
        case UnaryOperator::Minus:
            emit(OpCode::Negate);
            break;
        case UnaryOperator::Plus:
            break;
        case UnaryOperator::LogicalNot:
            emit(OpCode::Not);
            break;
        case UnaryOperator::BitwiseNot:
            emit(OpCode::BitwiseNot);
            break;
        case UnaryOperator::TypeOf:
            emit(OpCode::TypeOf);
            break;
        case UnaryOperator::VoidOp:
            emit(OpCode::Pop);
            emit(OpCode::LoadUndefined);
            break;
        default:
            break;
    }
}

void Compiler::compile_update_expression(const UpdateExpression& expr) {
    compile_expression(*expr.argument);

    emit(expr.op == UpdateOperator::Increment ? OpCode::PreIncrement : OpCode::PreDecrement);

    if (!expr.prefix) {
        emit(expr.op == UpdateOperator::Increment ? OpCode::PostIncrement : OpCode::PostDecrement);
    }
}

void Compiler::compile_binary_expression(const BinaryExpression& expr) {
    compile_expression(*expr.left);
    compile_expression(*expr.right);

    switch (expr.op) {
        case BinaryOperator::Add: emit(OpCode::Add); break;
        case BinaryOperator::Subtract: emit(OpCode::Subtract); break;
        case BinaryOperator::Multiply: emit(OpCode::Multiply); break;
        case BinaryOperator::Divide: emit(OpCode::Divide); break;
        case BinaryOperator::Modulo: emit(OpCode::Modulo); break;
        case BinaryOperator::BitwiseAnd: emit(OpCode::BitwiseAnd); break;
        case BinaryOperator::BitwiseOr: emit(OpCode::BitwiseOr); break;
        case BinaryOperator::BitwiseXor: emit(OpCode::BitwiseXor); break;
        case BinaryOperator::ShiftLeft: emit(OpCode::ShiftLeft); break;
        case BinaryOperator::ShiftRight: emit(OpCode::ShiftRight); break;
        case BinaryOperator::UnsignedShiftRight: emit(OpCode::UnsignedShiftRight); break;
        case BinaryOperator::Equal: emit(OpCode::Equal); break;
        case BinaryOperator::NotEqual: emit(OpCode::NotEqual); break;
        case BinaryOperator::StrictEqual: emit(OpCode::StrictEqual); break;
        case BinaryOperator::StrictNotEqual: emit(OpCode::StrictNotEqual); break;
        case BinaryOperator::Less: emit(OpCode::Less); break;
        case BinaryOperator::LessEqual: emit(OpCode::LessEqual); break;
        case BinaryOperator::Greater: emit(OpCode::Greater); break;
        case BinaryOperator::GreaterEqual: emit(OpCode::GreaterEqual); break;
        case BinaryOperator::In: emit(OpCode::In); break;
        case BinaryOperator::InstanceOf: emit(OpCode::InstanceOf); break;
        default:
            break;
    }
}

void Compiler::compile_logical_expression(const LogicalExpression& expr) {
    compile_expression(*expr.left);

    usize end_jump = emit_jump(expr.op == LogicalOperator::Or
                                   ? OpCode::JumpIfTrue
                                   : OpCode::JumpIfFalse);
    emit(OpCode::Pop);
    compile_expression(*expr.right);
    patch_jump(end_jump);
}

void Compiler::compile_conditional_expression(const ConditionalExpression& expr) {
    compile_expression(*expr.test);
    usize jump_to_else = emit_jump(OpCode::JumpIfFalse);
    emit(OpCode::Pop);
    compile_expression(*expr.consequent);
    usize jump_to_end = emit_jump(OpCode::Jump);
    patch_jump(jump_to_else);
    emit(OpCode::Pop);
    compile_expression(*expr.alternate);
    patch_jump(jump_to_end);
}

void Compiler::compile_assignment_expression(const AssignmentExpression& expr) {
    if (auto* id = dynamic_cast<const Identifier*>(expr.left.get())) {
        i32 local = resolve_local(m_current, id->name);
        i32 upvalue = -1;
        if (local == -1) {
            upvalue = resolve_upvalue(m_current, id->name);
        }

        if (expr.op == AssignmentOperator::Assign) {
            compile_expression(*expr.right);
        } else {
            if (local != -1) {
                emit(OpCode::GetLocal);
                emit(static_cast<u8>(local));
            } else if (upvalue != -1) {
                emit(OpCode::GetUpvalue);
                emit(static_cast<u8>(upvalue));
            } else {
                u16 idx = identifier_constant(id->name);
                emit(OpCode::GetGlobal);
                emit(static_cast<u8>(idx));
            }

            compile_expression(*expr.right);

            switch (expr.op) {
                case AssignmentOperator::AddAssign: emit(OpCode::Add); break;
                case AssignmentOperator::SubtractAssign: emit(OpCode::Subtract); break;
                case AssignmentOperator::MultiplyAssign: emit(OpCode::Multiply); break;
                case AssignmentOperator::DivideAssign: emit(OpCode::Divide); break;
                case AssignmentOperator::ModuloAssign: emit(OpCode::Modulo); break;
                case AssignmentOperator::BitwiseAndAssign: emit(OpCode::BitwiseAnd); break;
                case AssignmentOperator::BitwiseOrAssign: emit(OpCode::BitwiseOr); break;
                case AssignmentOperator::BitwiseXorAssign: emit(OpCode::BitwiseXor); break;
                case AssignmentOperator::ShiftLeftAssign: emit(OpCode::ShiftLeft); break;
                case AssignmentOperator::ShiftRightAssign: emit(OpCode::ShiftRight); break;
                case AssignmentOperator::UnsignedShiftRightAssign: emit(OpCode::UnsignedShiftRight); break;
                default: break;
            }
        }

        if (local != -1) {
            emit(OpCode::SetLocal);
            emit(static_cast<u8>(local));
        } else if (upvalue != -1) {
            emit(OpCode::SetUpvalue);
            emit(static_cast<u8>(upvalue));
        } else {
            u16 idx = identifier_constant(id->name);
            emit(OpCode::SetGlobal);
            emit(static_cast<u8>(idx));
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

} // namespace lithium::js
