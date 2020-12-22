/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef CUESTATE_CUESTATE_HPP_
#define CUESTATE_CUESTATE_HPP_

#include <type_traits>
#include <tuple>

namespace cue {
namespace state {

namespace detail {

template <typename _Ty>
concept is_null = std::is_null_pointer_v<_Ty>;

template <typename... _Args>
struct concat;

template <typename _Ty, typename... _Args>
struct concat<_Ty, std::tuple<_Args...>> {
    using type = std::tuple<_Ty, _Args...>;
};

template <typename _Ty>
struct concat<_Ty> {
    using type = std::tuple<_Ty>;
};

template <typename... _Args>
using concat_t = typename concat<_Args...>::type;

template <typename... _Args>
struct concat_tuple;

template <typename... _LArgs, typename... _RArgs>
struct concat_tuple<std::tuple<_LArgs...>, std::tuple<_RArgs...>> {
    using type = std::tuple<_LArgs..., _RArgs...>;
};

template <typename... _Args>
using concat_tuple_t = typename concat_tuple<_Args...>::type;

template <template <typename> class _Predicate, typename... _Args>
struct select_types;

template <template <typename> class _Predicate, typename _Ty, typename... _Args>
struct select_types<_Predicate, _Ty, _Args...> {
    using type =
        std::conditional_t<_Predicate<_Ty>::value, concat_t<_Ty, typename select_types<_Predicate, _Args...>::type>,
                           typename select_types<_Predicate, _Args...>::type>;
};

template <template <typename> class _Predicate>
struct select_types<_Predicate> {
    using type = std::tuple<>;
};

template <template <typename> class _Predicate, typename... _Args>
using select_types_t = typename select_types<_Predicate, _Args...>::type;

template <typename _Tuple, typename _Ty>
struct has_type;

template <typename _Head, typename... _Args, typename _Ty>
struct has_type<std::tuple<_Head, _Args...>, _Ty> : has_type<std::tuple<_Args...>, _Ty> {};

template <typename... _Args, typename _Ty>
struct has_type<std::tuple<_Ty, _Args...>, _Ty> : std::true_type {};

template <typename _Ty>
struct has_type<std::tuple<>, _Ty> : std::false_type {};

template <typename _Tuple, typename _Ty>
inline constexpr bool has_type_v = has_type<_Tuple, _Ty>::value;

template <typename _Ty, typename _Arg>
struct unique_type;

template <typename... _Ts, typename _Head, typename... _Args>
struct unique_type<std::tuple<_Ts...>, std::tuple<_Head, _Args...>> {
    using type = std::conditional_t<has_type_v<std::tuple<_Ts...>, _Head>,
                                    typename unique_type<std::tuple<_Ts...>, std::tuple<_Args...>>::type,
                                    typename unique_type<std::tuple<_Ts..., _Head>, std::tuple<_Args...>>::type>;
};

template <typename _Ty>
struct unique_type<_Ty, std::tuple<>> {
    using type = _Ty;
};

template <typename _Ty, typename _Arg>
using unique_type_t = typename unique_type<_Ty, _Arg>::type;

template <typename _Ty>
using unique_type_tuple = unique_type_t<std::tuple<>, _Ty>;

template <typename _Ty, typename _Tuple>
struct type_index;

template <typename _Ty, typename... _Args>
struct type_index<_Ty, std::tuple<_Ty, _Args...>> : public std::integral_constant<std::size_t, 0> {};

template <typename _Ty, typename _Head, typename... _Args>
struct type_index<_Ty, std::tuple<_Head, _Args...>>
    : public std::integral_constant<std::size_t, 1 + type_index<_Ty, std::tuple<_Args...>>{}> {};

template <typename _Ty, typename _Tuple>
inline constexpr std::size_t type_index_v = type_index<_Ty, _Tuple>::value;

} // namespace detail

template <typename _CurrentState, typename _Event, typename _TargetState, auto _Action, auto _Guard = nullptr>
struct transition final {
    static_assert(!std::is_same_v<_CurrentState, _TargetState>, "same states");

    using current_state = _CurrentState;
    using event = _Event;
    using target_state = _TargetState;

    static bool on(const _Event& event) {
        if constexpr (detail::is_null<decltype(_Guard)>) {
            _Action(event);
            return true;
        } else {
            if (_Guard) {
                if (!_Guard(event)) {
                    return false;
                }
            }
            _Action(event);
            return true;
        }
    }
};

template <typename... _Args>
struct table final {
    static_assert(sizeof...(_Args) > 0, "empty transition table");

    using transitions = std::tuple<_Args...>;
};

template <typename _Ty>
concept machine_t = requires {
    typename _Ty::initial_state;
    typename _Ty::transition_table;
    typename _Ty::transition_table::transitions;
};

template <typename _Ty>
concept transtion_t = requires {
    typename _Ty::current_state;
    typename _Ty::target_state;
};

template <machine_t _Machine>
class machine final {
    using initial_state = typename _Machine::initial_state;
    using transition_table = typename _Machine::transition_table;

    template <typename _Transitions>
    struct make_state_list;

    template <typename _Ty, typename... _Args>
    struct make_state_list<std::tuple<_Ty, _Args...>> {
        using type = detail::concat_tuple_t<std::tuple<typename _Ty::current_state, typename _Ty::target_state>,
                                            typename make_state_list<std::tuple<_Args...>>::type>;
    };

    template <typename _Ty>
    struct make_state_list<std::tuple<_Ty>> {
        using type = std::tuple<typename _Ty::current_state, typename _Ty::target_state>;
    };

    template <typename _Transitions>
    using make_state_list_t = typename make_state_list<_Transitions>::type;

    using states = detail::unique_type_tuple<make_state_list_t<typename transition_table::transitions>>;
    static_assert(detail::has_type_v<states, initial_state>, "bad initial state");

    std::size_t state_index_{detail::type_index_v<initial_state, states>};

    template <typename _Table, typename _Event>
    struct select_transitions;

    template <typename... _Args, typename _Event>
    struct select_transitions<std::tuple<_Args...>, _Event> {
        template <typename _Ty>
        using predicate = std::is_same<typename _Ty::event, _Event>;
        using type = detail::select_types_t<predicate, _Args...>;
    };

    template <typename _Table, typename _Event>
    using select_transitions_t = typename select_transitions<_Table, _Event>::type;

    template <typename _Transitions, typename _Event>
    struct dispatcher;

    template <transtion_t _Ty, typename... _Args, typename _Event>
    struct dispatcher<std::tuple<_Ty, _Args...>, _Event> {
        static bool on(std::size_t& state_index, const _Event& event) {
            constexpr auto index = detail::type_index_v<typename _Ty::current_state, states>;
            if (index == state_index) {
                if (_Ty::on(event)) {
                    constexpr auto target_state = detail::type_index_v<typename _Ty::target_state, states>;
                    state_index = target_state;
                    return true;
                }
                return false;
            }

            return dispatcher<std::tuple<_Args...>, _Event>::on(state_index, event);
        }
    };

    template <typename _Event>
    struct dispatcher<std::tuple<>, _Event> {
        static bool on(std::size_t& state_index, const _Event&) {
            // no transition
            return false;
        }
    };

public:
    machine() noexcept = default;
    ~machine() = default;

    machine(const machine&) = delete;
    machine& operator=(const machine&) = delete;

    template <typename _Event>
    bool on(const _Event& event) {
        using transitions = select_transitions_t<typename transition_table::transitions, _Event>;
        return dispatcher<transitions, _Event>::on(this->state_index_, event);
    }

    template <typename _State>
    bool is(const _State& state) const noexcept {
        static_assert(detail::has_type_v<states, _State>, "transtion table have no event");

        constexpr auto index = detail::type_index_v<_State, states>;
        return index == this->state_index_;
    }
};

} // namespace state
} // namespace cue

#endif // CUESTATE_CUESTATE_HPP_
