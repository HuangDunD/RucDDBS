#pragma once
#include <random>

namespace raft{
    class Random{
    public:
        Random(int seed,int left,int right)
            : engine(seed), dist(left,right) {}
        int generate(){
            return dist(engine);
        }
    private:
        std::default_random_engine engine;
        std::uniform_int_distribution<> dist;
    };
}

