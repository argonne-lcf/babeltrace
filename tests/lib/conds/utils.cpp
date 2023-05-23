/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020 Philippe Proulx <pproulx@efficios.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <babeltrace2/babeltrace.h>
#include <glib.h>
#include <iostream>

#include "common/assert.h"
#include "cpp-common/nlohmann/json.hpp"
#include "utils.hpp"

typedef void (*run_in_comp_cls_init_func)(bt_self_component *self_comp, void *user_data);

struct comp_cls_init_method_data
{
    run_in_comp_cls_init_func func;
    void *user_data;
};

static bt_component_class_initialize_method_status
comp_cls_init(bt_self_component_source *self_comp, bt_self_component_source_configuration *conf,
              const bt_value *params, void *init_method_data)
{
    comp_cls_init_method_data *data = static_cast<comp_cls_init_method_data *>(init_method_data);

    /* Call user function which is expected to abort */
    data->func(bt_self_component_source_as_self_component(self_comp), data->user_data);

    /* Never reached! */
    return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
}

static bt_message_iterator_class_next_method_status
msg_iter_cls_next(bt_self_message_iterator *self_msg_iter, bt_message_array_const msgs,
                  uint64_t capacity, uint64_t *count)
{
    /* Not used */
    return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
}

static void run_in_comp_cls_init(run_in_comp_cls_init_func func, void *user_data)
{
    bt_message_iterator_class *msg_iter_cls;
    bt_component_class_source *comp_cls;
    bt_component_class_set_method_status set_method_status;
    bt_graph *graph;
    struct comp_cls_init_method_data init_method_data = {
        .func = func,
        .user_data = user_data,
    };

    /* Create component class */
    msg_iter_cls = bt_message_iterator_class_create(msg_iter_cls_next);
    BT_ASSERT(msg_iter_cls);
    comp_cls = bt_component_class_source_create("yo", msg_iter_cls);
    BT_ASSERT(comp_cls);
    set_method_status = bt_component_class_source_set_initialize_method(comp_cls, comp_cls_init);
    BT_ASSERT(set_method_status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);

    /* Create graph */
    graph = bt_graph_create(0);
    BT_ASSERT(graph);

    /*
	 * Add source component: this calls the initialization method,
	 * calling `func`.
	 */
    (void) bt_graph_add_source_component_with_initialize_method_data(
        graph, comp_cls, "whatever", NULL, &init_method_data, BT_LOGGING_LEVEL_NONE, NULL);

    /*
	 * This point is not expected to be reached as func() is
	 * expected to abort.
	 */
}

static void run_in_comp_cls_init_defer(bt_self_component *self_comp, void *user_data)
{
    cond_trigger_run_in_comp_cls_init_func user_func =
        reinterpret_cast<cond_trigger_run_in_comp_cls_init_func>(user_data);

    user_func(self_comp);
}

static void run_trigger(const struct cond_trigger *trigger)
{
    switch (trigger->func_type) {
    case COND_TRIGGER_FUNC_TYPE_BASIC:
        trigger->func.basic();
        break;
    case COND_TRIGGER_FUNC_TYPE_RUN_IN_COMP_CLS_INIT:
        run_in_comp_cls_init(run_in_comp_cls_init_defer,
                             reinterpret_cast<void *>(trigger->func.run_in_comp_cls_init));
        break;
    default:
        abort();
    }
}

static void list_triggers(const struct cond_trigger triggers[], size_t trigger_count)
{
    nlohmann::json trigger_array = nlohmann::json::array();

    for (size_t i = 0; i < trigger_count; i++) {
        nlohmann::json trigger_obj = nlohmann::json::object();
        const cond_trigger& trigger = triggers[i];

        /* Condition ID */
        trigger_obj["cond-id"] = trigger.cond_id;

        /* Name starts with condition ID */
        std::string name = trigger.cond_id;

        if (trigger.suffix) {
            name += '-';
            name += trigger.suffix;
        }

        trigger_obj["name"] = std::move(name);
        trigger_array.push_back(std::move(trigger_obj));
    }

    auto str = trigger_array.dump();
    std::cout << str;
    std::flush(std::cout);
}

void cond_main(int argc, const char *argv[], const struct cond_trigger triggers[],
               size_t trigger_count)
{
    BT_ASSERT(argc >= 2);

    if (strcmp(argv[1], "list") == 0) {
        list_triggers(triggers, trigger_count);
    } else if (strcmp(argv[1], "run") == 0) {
        int index;

        BT_ASSERT(argc >= 3);
        index = atoi(argv[2]);
        BT_ASSERT(index >= 0 && index < trigger_count);
        run_trigger(&triggers[index]);
    }
}
