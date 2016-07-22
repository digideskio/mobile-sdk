/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_MAPNIKVT_EXPRESSIONBINDER_H_
#define _CARTO_MAPNIKVT_EXPRESSIONBINDER_H_

#include "Expression.h"
#include "ExpressionContext.h"
#include "ValueConverter.h"
#include "vt/Color.h"
#include "vt/Styles.h"

#include <memory>
#include <functional>

namespace carto { namespace mvt {
    template <typename V>
    class ExpressionBinder {
    public:
        ExpressionBinder() = default;

        ExpressionBinder& bind(V* field, const std::shared_ptr<const Expression>& expr, std::function<V(const Value& val)> convertFn) {
            if (auto constExpr = std::dynamic_pointer_cast<const ConstExpression>(expr)) {
                *field = convertFn(constExpr->getConstant());
            }
            else {
                _bindingMap.insert({ field, Binding(expr, std::move(convertFn)) });
            }
            return *this;
        }

        void update(const FeatureExpressionContext& context) const {
            for (auto it = _bindingMap.begin(); it != _bindingMap.end(); it++) {
                const Binding& binding = it->second;
                Value val = binding.expr->evaluate(context);
                *it->first = binding.convertFn(val);
            }
        }

    private:
        struct Binding {
            std::shared_ptr<const Expression> expr;
            std::function<V(const Value&)> convertFn;

            explicit Binding(std::shared_ptr<const Expression> expr, std::function<V(const Value&)> convertFn) : expr(std::move(expr)), convertFn(std::move(convertFn)) { }
        };

        std::map<V*, Binding> _bindingMap;
    };

    template <typename V>
    class ExpressionFunctionBinder {
    public:
        ExpressionFunctionBinder() = default;

        ExpressionFunctionBinder& bind(std::shared_ptr<const V>* field, const std::shared_ptr<const Expression>& expr) {
            if (auto constExpr = std::dynamic_pointer_cast<const ConstExpression>(expr)) {
                *field = buildFunction(expr);
            }
            else {
                _bindingMap.insert({ field, expr });
            }
            return *this;
        }

        void update(const FeatureExpressionContext& context) const {
            for (auto it = _bindingMap.begin(); it != _bindingMap.end(); it++) {
                std::shared_ptr<const Expression> expr = simplifyExpression(it->second, context);
                *it->first = buildFunction(expr);
            }
        }

    private:
        std::shared_ptr<const V> buildFunction(const std::shared_ptr<const Expression>& expr) const {
            for (auto it = _functionCache.begin(); it != _functionCache.end(); it++) {
                if (it->first->equals(expr)) {
                    return it->second;
                }
            }
            auto func = std::make_shared<const V>([expr](const vt::ViewState& viewState) {
                ViewExpressionContext context;
                context.setZoom(viewState.zoom);
                return ValueConverter<float>::convert(expr->evaluate(context));
            });
            if (_functionCache.size() > 16) {
                _functionCache.erase(_functionCache.begin()); // erase any element to keep cache compact
            }
            _functionCache[expr] = func;
            return func;
        }

        static std::shared_ptr<const Expression> simplifyExpression(const std::shared_ptr<const Expression>& expr, const FeatureExpressionContext& context) {
            return expr->map([&context](const std::shared_ptr<const Expression>& expr) -> std::shared_ptr<const Expression> {
                bool containsViewVariables = false;
                expr->fold([&containsViewVariables, &context](const std::shared_ptr<const Expression>& expr) {
                    if (auto varExpr = std::dynamic_pointer_cast<const VariableExpression>(expr)) {
                        std::string name = varExpr->getVariableName(context);
                        if (ViewExpressionContext::isViewVariable(name)) {
                            containsViewVariables = true;
                        }
                    }
                });
                if (containsViewVariables) {
                    return expr;
                }
                Value val = expr->evaluate(context);
                return std::make_shared<ConstExpression>(val);
            });
        }

        std::map<std::shared_ptr<const V>*, std::shared_ptr<const Expression>> _bindingMap;
        mutable std::map<std::shared_ptr<const Expression>, std::shared_ptr<const V>> _functionCache;
    };
} }

#endif
