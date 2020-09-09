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

template <typename... Args>
struct concat;

template <typename T, typename... Args>
struct concat<T, std::tuple<Args...>> {
    using type = std::tuple<T, Args...>;
};

template <typename T>
struct concat<T> {
    using type = std::tuple<T>;
};

template <typename... Args>
using concat_t = typename concat<Args...>::type;

template <typename... Args>
struct concat_tuple;

template <typename... LArgs, typename... RArgs>
struct concat_tuple<std::tuple<LArgs...>, std::tuple<RArgs...>> {
    using type = std::tuple<LArgs..., RArgs...>;
};

template <typename... Args>
using concat_tuple_t = typename concat_tuple<Args...>::type;

template <template <typename> class Predicate, typename... Args>
struct select_types;

template <template <typename> class Predicate, typename T, typename... Args>
struct select_types<Predicate, T, Args...> {
    using type = std::conditional_t<Predicate<T>::value, concat_t<T, typename select_types<Predicate, Args...>::type>,
                                    typename select_types<Predicate, Args...>::type>;
};

template <template <typename> class Predicate>
struct select_types<Predicate> {
    using type = std::tuple<>;
};

template <template <typename> class Predicate, typename... Args>
using select_types_t = typename select_types<Predicate, Args...>::type;

template <typename Tuple, typename T>
struct has_type;

template <typename Head, typename... Args, typename T>
struct has_type<std::tuple<Head, Args...>, T> : has_type<std::tuple<Args...>, T> {};

template <typename... Args, typename T>
struct has_type<std::tuple<T, Args...>, T> : std::true_type {};

template <typename T>
struct has_type<std::tuple<>, T> : std::false_type {};

template <typename Tuple, typename T>
inline constexpr bool has_type_v = has_type<Tuple, T>::value;

template <typename Out, typename In>
struct unique_type;

template <typename... Out, typename T, typename... Args>
struct unique_type<std::tuple<Out...>, std::tuple<T, Args...>> {
    using type = std::conditional_t<has_type_v<std::tuple<Out...>, T>,
                                    typename unique_type<std::tuple<Out...>, std::tuple<Args...>>::type,
                                    typename unique_type<std::tuple<Out..., T>, std::tuple<Args...>>::type>;
};

template <typename Out>
struct unique_type<Out, std::tuple<>> {
    using type = Out;
};

template <typename Out, typename In>
using unique_type_t = typename unique_type<Out, In>::type;

template <typename T>
using unique_type_tuple = unique_type_t<std::tuple<>, T>;

template <typename T, typename Tuple>
struct type_index;

template <typename T, typename... Args>
struct type_index<T, std::tuple<T, Args...>> : public std::integral_constant<std::size_t, 0> {};

template <typename T, typename U, typename... Args>
struct type_index<T, std::tuple<U, Args...>>
    : public std::integral_constant<std::size_t, 1 + type_index<T, std::tuple<Args...>>{}> {};

template <typename T, typename Tuple>
inline constexpr std::size_t type_index_v = type_index<T, Tuple>::value;

} // namespace detail

template <typename CurrentState, typename Event, typename TargetState, auto Action, auto Guard = nullptr>
struct transition final {
    static_assert(!std::is_same_v<CurrentState, TargetState>, "same states");

    using current_state = CurrentState;
    using event = Event;
    using target_state = TargetState;

    static bool on(const Event& event) {
        if constexpr (std::is_same_v<decltype(Guard), std::nullptr_t>) {
            Action(event);
            return true;
        } else {
            if (Guard) {
                if (!Guard(event)) {
                    return false;
                }
            }
            Action(event);
            return true;
        }
    }
};

template <typename... Args>
struct table final {
    static_assert(sizeof...(Args) > 0, "empty transition table");

    using transitions = std::tuple<Args...>;
};

template <typename Machine>
class machine final {
    using initial_state = typename Machine::initial_state;
    using transition_table = typename Machine::transition_table;

    template <typename Transitions>
    struct make_state_list;

    template <typename T, typename... Args>
    struct make_state_list<std::tuple<T, Args...>> {
        using type = detail::concat_tuple_t<std::tuple<typename T::current_state, typename T::target_state>,
                                            typename make_state_list<std::tuple<Args...>>::type>;
    };

    template <typename T>
    struct make_state_list<std::tuple<T>> {
        using type = std::tuple<typename T::current_state, typename T::target_state>;
    };

    using states = detail::unique_type_tuple<typename make_state_list<typename transition_table::transitions>::type>;
    static_assert(detail::has_type_v<states, initial_state>, "bad initial state");

    std::size_t state_index_{detail::type_index_v<initial_state, states>};

    template <typename Table, typename Event>
    struct select_transitions;

    template <typename... Args, typename Event>
    struct select_transitions<std::tuple<Args...>, Event> {
        template <typename T>
        using predicate = std::is_same<typename T::event, Event>;
        using type = detail::select_types_t<predicate, Args...>;
    };

    template <typename Table, typename Event>
    using select_transitions_t = typename select_transitions<Table, Event>::type;

    template <typename Transitions, typename Event>
    struct dispatcher;

    template <typename T, typename... Args, typename Event>
    struct dispatcher<std::tuple<T, Args...>, Event> {
        static bool on(std::size_t& state_index, const Event& event) {
            constexpr auto index = detail::type_index_v<typename T::current_state, states>;
            if (index == state_index) {
                if (T::on(event)) {
                    constexpr auto target_state = detail::type_index_v<typename T::target_state, states>;
                    state_index = target_state;
                    return true;
                }
                return false;
            }

            return dispatcher<std::tuple<Args...>, Event>::on(state_index, event);
        }
    };

    template <typename Event>
    struct dispatcher<std::tuple<>, Event> {
        static bool on(std::size_t& state_index, const Event&) {
            // no transition
            return false;
        }
    };

public:
    machine() noexcept = default;
    ~machine() = default;

    template <typename Event>
    bool on(const Event& event) {
        using transitions = select_transitions_t<typename transition_table::transitions, Event>;
        return dispatcher<transitions, Event>::on(this->state_index_, event);
    }

    template <typename State>
    bool is(const State& state) const {
        static_assert(detail::has_type_v<states, State>, "transtion table have no event");
        constexpr auto index = detail::type_index_v<State, states>;
        return index == this->state_index_;
    }
};

} // namespace state
} // namespace cue

#endif // CUESTATE_CUESTATE_HPP_
