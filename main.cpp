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

#include <iostream>
#include <cassert>

#include "cuestate.hpp"

using namespace cue::state;

// states
struct closed {};
struct opened {};
struct walking {};

// events
struct close {};
struct open {};
struct walk {
    bool is_ready_;
    int distance_;
};
struct stop {};

// actions
const auto do_open = [](const open&) { std::cout << "open" << std::endl; };
const auto do_close = [](const close&) { std::cout << "close" << std::endl; };
const auto do_stop = [](const stop&) { std::cout << "stop" << std::endl; };
const auto do_walk = [](const walk& w) { std::cout << "walking " << w.distance_ << "m" << std::endl; };

// guards
const auto is_ready = [](const walk& w) {
    std::cout << "robot is ready" << std::endl;
    return w.is_ready_;
};

struct light_robot {
    using initial_state = closed;
    using transition_table = table<
        //        +---------+----------+----------+---------------+---------------+
        //        | current |   event  |  target  |     action    |guard(optional)|
        //        +---------+----------+----------+---------------+---------------+
        transition< closed  ,   open   ,  opened  ,    do_open                    >,
        transition< opened  ,   close  ,  closed  ,    do_close                   >,
        transition< opened  ,   walk   ,  walking ,    do_walk    ,    is_ready   >,
        transition< walking ,   stop   ,  opened  ,    do_stop                    >,
        transition< walking ,   close  ,  closed  ,    do_close                   >
    >;
};

int main(int argc, char** argv) {
    machine<light_robot> robot;
    assert(robot.is(closed{}));
    robot.on(open{});
    assert(robot.is(opened{}));
    robot.on(close{});
    assert(robot.is(closed{}));
    robot.on(open{});
    assert(robot.is(opened{}));
    robot.on(walk{ .is_ready_ = true, .distance_ = 5 });
    assert(robot.is(walking{}));
    robot.on(stop{});
    assert(robot.is(opened{}));
    robot.on(close{});
    assert(robot.is(closed{}));
    robot.on(stop{});
    assert(robot.is(closed{}));

    return 0;
}
