#ifndef _FSM_PARSER_H
#define _FSM_PARSER_H

#include "data_structures.h"

int
fsm_parse(
        char *input,
        FsmProcess *process
        );

int
fsm_print(
        const FsmProcess *process
        );

#endif