#pragma once 
#include "trajectory.h"
class Evaluator{
    public:
        virtual ~Evaluator() = default;
        virtual double evaluate(const Trajectory& trajectory) = 0;
};