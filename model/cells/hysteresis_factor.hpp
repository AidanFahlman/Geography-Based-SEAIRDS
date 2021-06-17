//
// Created by binybrion on 8/12/20.
//

#ifndef PANDEMIC_HOYA_2002_HYSTERESIS_FACTOR_HPP
#define PANDEMIC_HOYA_2002_HYSTERESIS_FACTOR_HPP

struct hysteresis_factor
{
    bool in_effect = false;
    float mobility_correction_factor = 1.0f;
    float infections_higher_bound = 0.0f;       // Infection threshold of next correction factor
    float infections_lower_bound = 0.0f;        // Infection threshold of hysteresis adjusted current correction factor
};

#endif //PANDEMIC_HOYA_2002_HYSTERESIS_FACTOR_HPP
