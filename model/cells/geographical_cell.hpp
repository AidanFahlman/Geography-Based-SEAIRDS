//
// Created by binybrion on 6/29/20.
// Modified by Glenn 02/07/20

#ifndef PANDEMIC_HOYA_2002_ZHONG_CELL_HPP
#define PANDEMIC_HOYA_2002_ZHONG_CELL_HPP

#include <cmath>
#include <iostream>
#include <vector>
#include <cadmium/celldevs/cell/cell.hpp>
#include <iomanip>
#include "vicinity.hpp"
#include "seaird.hpp"
#include "simulation_config.hpp"

using namespace std;
using namespace cadmium::celldevs;

template <typename T>
class geographical_cell : public cell<T, std::string, seaird, vicinity> {
public:

    template <typename X>
    using cell_unordered = std::unordered_map<std::string, X>;

    using cell<T, std::string, seaird, vicinity>::simulation_clock;
    using cell<T, std::string, seaird, vicinity>::state;
    using cell<T, std::string, seaird, vicinity>::neighbors;
    using cell<T, std::string, seaird, vicinity>::cell_id;

    using config_type = simulation_config;

    using phase_rates = std::vector<            // The age sub_division
                        std::vector<double>>;   // The stage of infection

    phase_rates virulence_rates;
    phase_rates incubation_rates;
    phase_rates recovery_rates;
    phase_rates mobility_rates;
    phase_rates fatality_rates;
    double asymptomatic_rates;

    // To make the parameters of the correction_factors variable more obvious
    using infection_threshold = float;
    using mobility_correction_factor = std::array<float, 2>; // The first value is the mobility correction factor;
                                                             // The second one is the hysteresis factor.

    int prec_divider;
    bool SIIRS_model = true;

    geographical_cell() : cell<T, std::string, seaird, vicinity>() {}

    geographical_cell(std::string const &cell_id, cell_unordered<vicinity> const &neighborhood,
                      seaird const &initial_state, std::string const &delay_id, simulation_config config) :
    cell<T, std::string, seaird, vicinity>(cell_id, neighborhood, initial_state, delay_id) {

        for(const auto &i : neighborhood) {
            state.current_state.hysteresis_factors.insert({i.first, hysteresis_factor{}});
        }

        virulence_rates = std::move(config.virulence_rates);
        incubation_rates = std::move(config.incubation_rates);
        recovery_rates = std::move(config.recovery_rates);
        mobility_rates = std::move(config.mobility_rates);
        fatality_rates = std::move(config.fatality_rates);
        asymptomatic_rates = std::move(config.asymptomatic_rates);

        prec_divider = config.prec_divider;
        SIIRS_model = config.SIIRS_model;

        assert(virulence_rates.size() == recovery_rates.size() && virulence_rates.size() == mobility_rates.size() &&
               virulence_rates.size() == incubation_rates.size() &&
               "\n\nThere must be an equal number of age segments between all configuration rates.\n\n");
    }

    // Whenever referring to a "population", it is meant the current age group's population.
    // The state of each age group's population is calculated individually.
    seaird local_computation() const override {

        seaird res = state.current_state;

        // calculate the next new seaird variables for each age group
        for(int age_segment_index = 0; age_segment_index < res.get_num_age_segments(); ++age_segment_index) {

            // Note: Remember that these recoveries and fatalities are from the previous simulation cycle. Thus there is an ordering
            // issue- recovery rate and fatality rates specify a percentage of the infected at a certain stage. As a result
            // the code cannot for example, calculate recovery, change the infected stage population, and then calculate
            // fatalities, or vice-versa. This would change the meaning of the input.

            // Calculate fatalities BEFORE recoveries. This avoids the following problem: Due to the fatality modifier (when
            // hospitals are overwhelmed), a check to make sure fatalities do not exceed the infected population has to be done.
            // This conflicts with the recovery logic, where it is assumed that all infected on the last stage recover. As a result,
            // the logic to ensure that the recovered + fatalities for the last stage do not exceed the last stage infected population
            // results in no fatalities being possible on the last stage of infection (as fatalities would have to be capped at
            // infected population [for a stage] - recovered population [for a stage]. But the last stage of recovered
            // was already set to the population of last stage of infected- meaning fatalities is always 0 for the last stage).

            // cauculate the total number of new exposed entering exposed(0)
            double new_e = std::round(new_exposed(age_segment_index, res) * prec_divider) / prec_divider;

            // calculate the total number new infected, exposed last day + exposed other days becoming infected
            double new_i = std::round(new_infections(age_segment_index, res) * prec_divider) / prec_divider;

            // calculate the total number new asymptomatic, exposed last day + exposed other days becoming infected
            double new_a = std::round(new_asymptomatic(age_segment_index, res) * prec_divider) / prec_divider;

            // calculate the vector of fatalities entered from each infection day
            std::vector<double> fatalities = new_fatalities(res, age_segment_index);

            // calculate the vector of new recoveries entering from each infection day 1:num_infection_phases,
            std::vector<double> recovered = new_recoveries(res, age_segment_index, fatalities);

            res.fatalities.at(age_segment_index) += std::accumulate(fatalities.begin(), fatalities.end(), 0.0f);

            // The susceptible population is smaller due to previous deaths
            double new_s = 1 - res.fatalities.at(age_segment_index);

            // So far, it was assumed that on the last day of infection, all recovered. But this is not true- have to account
            // for those who died on the last day of infection.
            recovered.back() -= fatalities.back();

            // Advance all exposed forward a day, with some proportion leaving exposed(q-1) and entering infected(1)
            for (int i = res.get_num_exposed_phases() - 1; i > 0; --i)
            {
                // calculate new exposed based on the incubation rate and the previous days exposed
                double curr_expos = std::round(res.exposed.at(age_segment_index).at(i - 1)
                    *(1-incubation_rates.at(age_segment_index).at(i-1))*prec_divider) / prec_divider;

                // The susceptible population does not include the exposed population
                new_s -= curr_expos;

                res.exposed.at(age_segment_index).at(i) = curr_expos;
            }
            res.exposed.at(age_segment_index).at(0) = new_e;
            new_s -= new_e;

            // Equation 6d
            // Advance all infected q = 0 to q = Ti-1 one day forward
            for (int i = res.get_num_infected_phases() - 1; i > 0; --i)
            {
                // *** Calculate proportion of infected on a given day of the infection ***

                // The previous day of infection
                double curr_inf = res.infected.at(age_segment_index).at(i - 1);
                double curr_asymp = res.asymptomatic.at(age_segment_index).at(i - 1);

                // The number of people in a stage of infection moving to the new infection stage do not include those
                // who have died or recovered. Note: A subtraction must be done here as the recovery and mortality rates
                // are given for the total population of an infection stage. Multiplying by (1 - respective rate) here will
                // NOT work as the second multiplication done will effectively be of the infection stage population after
                // the first multiplication, rather than the entire infection state population.
                curr_inf -= recovered.at(i - 1) * (1-asymptomatic_rates);
                curr_inf -= fatalities.at(i - 1);
                curr_asymp -= recovered.at(i - 1) * asymptomatic_rates;


                curr_inf = std::round(curr_inf * prec_divider) / prec_divider;
                curr_asymp = std::round(curr_asymp * prec_divider) / prec_divider;

                // The amount of susceptible does not include the infected population
                new_s -= curr_inf + curr_asymp;

                res.infected.at(age_segment_index).at(i) = curr_inf;
                res.asymptomatic.at(age_segment_index).at(i) = curr_asymp;
            }

            // The people on the first day of infection
            res.infected.at(age_segment_index).at(0) = new_i;
            res.asymptomatic.at(age_segment_index).at(0) = new_a;

            // The susceptible population does not include those that just became exposed
            new_s -= (new_i + new_a);

            int recovered_index = res.get_num_recovered_phases() - 1;

            if(!SIIRS_model) {
                // Add the population on the second last day of recovery to the population on the last day of recovery.
                // This entire population on the last day of recovery is then subtracted from the susceptible population
                // to take into account that the population on the last day of recovery will not be subtracted from the susceptible
                // population in the Equation 6a for loop.
                res.recovered.at(age_segment_index).back() += res.recovered.at(age_segment_index).at(res.get_num_recovered_phases() - 2);
                new_s -= res.recovered.at(age_segment_index).back();
                // Avoid processing the population on the last day of recovery in the equation 6a for loop. This will
                // update all stages of recovery population except the last one, which grows with every time step
                // as it is only added to from the population on the second last day of recovery.
                recovered_index -= 1;
            }

            // Equation 6a
            for(int i = recovered_index; i > 0; --i)
            {
                // Each day of the recovered is the value of the previous day. The population on the last day is
                // now susceptible (assuming a SIIRS model); this is implicitly done already as the susceptible value was set to 1.0 and the
                // population on the last day of recovery is never subtracted from the susceptible value.
                res.recovered.at(age_segment_index).at(i) = res.recovered.at(age_segment_index).at(i - 1);
                new_s -= res.recovered.at(age_segment_index).at(i);
            }

            // The people on the first day of recovery are those that were on the last stage of infection (minus those who died;
            // already accounted for) in the previous time step plus those that recovered early during an infection stage.
            res.recovered.at(age_segment_index).at(0) = std::accumulate(recovered.begin(), recovered.end(), 0.0f);

            // The susceptible population does not include the recovered population
            new_s -= std::accumulate(recovered.begin(), recovered.end(), 0.0f);

            if (new_s > -0.001 && new_s < 0) new_s = 0; // double precision issues
            assert(new_s >= 0);

            res.susceptible.at(age_segment_index) = new_s;
        }

        return res;
    }

    // It returns the delay to communicate cell's new state.
    T output_delay(seaird const &cell_state) const override {
        return 1;
    }
    
    double new_exposed(unsigned int age_segment_index, seaird &current_seaird) const {
        double expos = 0;
        double expos_i = 0;
        double expos_a = 0;
        seaird const cstate = state.current_state;

        // calculate the correction factor of the current cell
        // The current cell must be part of its own neighborhood for this to work!
        vicinity self_vicinity = state.neighbors_vicinity.at(cell_id);
        double current_cell_correction_factor = cstate.disobedient.at(age_segment_index)
        + (1 - cstate.disobedient.at(age_segment_index)) * movement_correction_factor(self_vicinity.correction_factors,
                                                    state.neighbors_state.at(cell_id).get_total_infections(),
                                                    current_seaird.hysteresis_factors.at(cell_id));

        // external exposed
        for(auto neighbor: neighbors) {
            seaird const nstate = state.neighbors_state.at(neighbor);
            vicinity v = state.neighbors_vicinity.at(neighbor);

            // disobedient people have a correction factor of 1. The rest of the population is affected by the movement_correction_factor
            double neighbor_correction = nstate.disobedient.at(age_segment_index) +
                    (1 - nstate.disobedient.at(age_segment_index)) *
                    movement_correction_factor(v.correction_factors,
                                               nstate.get_total_infections(),
                                               current_seaird.hysteresis_factors.at(neighbor));

            // Logically makes sense to require neighboring cells to follow the movement restriction that is currently
            // in place in the current cell if the current cell has a more restrictive movement.
            neighbor_correction = std::min(current_cell_correction_factor, neighbor_correction);

            for (int i = 0; i < nstate.get_num_infected_phases(); ++i) {
                /*expos += v.correlation * mobility_rates.at(age_segment_index).at(i) * // variable Cij
                         virulence_rates.at(age_segment_index).at(i) * // variable lambda
                         cstate.susceptible.at(age_segment_index) * //variable Si
                         ( nstate.get_total_asymptomatic() +
                           nstate.get_total_infections() ) * // variable Ij,
                         neighbor_correction;  // New exposed may be slightly fewer if there are mobility restrictions  */

                //NEW TESTING
                expos_i += v.correlation * mobility_rates.at(age_segment_index).at(i) * // variable Cij
                         virulence_rates.at(age_segment_index).at(i) * // variable lambda
                         cstate.susceptible.at(age_segment_index) * //variable Si
                         nstate.get_total_infections() * neighbor_correction; // New exposed may be slightly fewer if there are mobility restrictions


                expos_a += v.correlation * mobility_rates.at(age_segment_index).at(i) * // variable Cij
                         virulence_rates.at(age_segment_index).at(i) * // variable lambda
                         cstate.susceptible.at(age_segment_index) * //variable Si
                         nstate.get_total_asymptomatic();  // We only consider mobility restrictions for those who are symptomatic

                expos = expos_i + expos_a;


            }
        }
        return std::min(cstate.susceptible.at(age_segment_index), expos);
    }

    double new_infections(unsigned int age_segment_index, seaird &current_seaird) const {
        double inf = 0;
        seaird const cstate = state.current_state;
        inf = cstate.exposed.at(age_segment_index).back();

        // scan through all exposed day except last and calculate exposed.at(asi).at(i)
        for(int i = 0; i < cstate.exposed.at(age_segment_index).size() - 1 ; i++){
            inf += cstate.exposed.at(age_segment_index).at(i) * incubation_rates.at(age_segment_index).at(i);
        }
        inf = std::round(((1-asymptomatic_rates) * inf) * prec_divider) / prec_divider;
        return inf;
    }

    double new_asymptomatic(unsigned int age_segment_index, seaird &current_seaird) const {
        double asym = 0;
        seaird const cstate = state.current_state;
        asym = cstate.exposed.at(age_segment_index).back();

        // scan through all exposed day except last and calculate exposed.at(asi).at(i)
        for(int i = 0; i < cstate.exposed.at(age_segment_index).size() - 1 ; i++){
            asym += cstate.exposed.at(age_segment_index).at(i) * incubation_rates.at(age_segment_index).at(i);
        }
        asym = std::round( (asymptomatic_rates * asym) * prec_divider) / prec_divider;
        return asym;
    }

    std::vector<double> new_recoveries(const seaird &current_state, unsigned int age_segment_index, const std::vector<double> &fatalities) const {
        std::vector<double> recovered(current_state.get_num_infected_phases(), 0.0f);

        // Assume that any individuals that are not fatalities on the last stage of infection recover
        recovered.back() =
                (current_state.infected.at(age_segment_index).back() +
                 current_state.asymptomatic.at(age_segment_index).back()) -
                 fatalities.back();

        for (int i = 0; i < current_state.get_num_infected_phases() - 1; ++i) {
            // Calculate all of the new recovered- for every day that a population is infected, some recover.
            float new_recoveries = std::round(
                    (current_state.infected.at(age_segment_index).at(i) + current_state.asymptomatic.at(age_segment_index).at(i) ) *
                    recovery_rates.at(age_segment_index).at(i) * prec_divider) / prec_divider;

            // There can't be more recoveries than those who have died
            float maximum_possible_recoveries = (
                    current_state.infected.at(age_segment_index).at(i) +
                    current_state.asymptomatic.at(age_segment_index).at(i) ) -
                    fatalities.at(i);

            recovered.at(i) = std::min(new_recoveries, maximum_possible_recoveries);
        }

        return recovered;
    }

    std::vector<double> new_fatalities(const seaird &current_state, unsigned int age_segment_index) const {
        std::vector<double> fatalities(current_state.get_num_infected_phases(), 0.0f);

        // Calculate all those who have died during an infection stage.
        for(int i = 0; i < current_state.get_num_infected_phases(); ++i) {
            // Fatalities are only considered for those who are symptomatic(infected). It is extremely rare for an
            // asympyomatic patient to die as a result of COVID-19
            fatalities.at(i) += std::round(
                    current_state.infected.at(age_segment_index).at(i) *
                    fatality_rates.at(age_segment_index).at(i) * prec_divider) / prec_divider;


            if(current_state.get_total_infections() > current_state.hospital_capacity) {
                fatalities.at(i) *= current_state.fatality_modifier;
            }

            // There can't be more fatalities than the number of people who are infected at a stage
            fatalities.at(i) = std::min(fatalities.at(i), (current_state.infected.at(age_segment_index).at(i) + current_state.asymptomatic.at(age_segment_index).at(i) ));
        }

        return fatalities;
    }

    float movement_correction_factor(const std::map<infection_threshold, mobility_correction_factor> &mobility_correction_factors,
                                     float infectious_population, hysteresis_factor &hysteresisFactor) const {

        // For example, assume a correction factor of "0.4": [0.2, 0.1]. If the infection goes above 0.4, then the
        // correction factor of 0.2 will now be applied to total infection values above 0.3, no longer 0.4 as the
        // hysteresis is in effect.
        if(infectious_population > hysteresisFactor.infections_higher_bound) {
            hysteresisFactor.in_effect = false;
        }

        // This is uses the comparison '>', not '>=' ; otherwise if the lower bound is 0 there is no way for the hysteresis
        // to disappear as the infections can never go below 0
        if(hysteresisFactor.in_effect && infectious_population > hysteresisFactor.infections_lower_bound) {
            return hysteresisFactor.mobility_correction_factor;
        }

        hysteresisFactor.in_effect = false;

        float correction = 1.0f;
        for (auto const &pair: mobility_correction_factors) {
            if (infectious_population >= pair.first) {
                correction = pair.second.front();

                // A hysteresis factor will be in effect until the total infection goes below the hysteresis factor;
                // until that happens the information required to return a movement factor must be kept in above variables.

                // Get the threshold of the next correction factor; otherwise the current correction factor can remain in
                // effect if the total infections never goes below the lower bound hysteresis factor, but also if it goes
                // above the original total infection threshold!
                auto next_pair_iterator = std::find(mobility_correction_factors.begin(), mobility_correction_factors.end(), pair);
                assert(next_pair_iterator != mobility_correction_factors.end());

                // If there is a next correction factor (for a higher total infection), then use it's total infection threshold
                if(std::distance(mobility_correction_factors.begin(), next_pair_iterator) != mobility_correction_factors.size() - 1) {
                    ++next_pair_iterator;
                }

                hysteresisFactor.in_effect = true;
                hysteresisFactor.infections_higher_bound = next_pair_iterator->first;
                hysteresisFactor.infections_lower_bound = pair.first - pair.second.back();
                hysteresisFactor.mobility_correction_factor = pair.second.front();
            } else {
                break;
            }
        }
        return correction;
    }
};

#endif //PANDEMIC_HOYA_2002_ZHONG_CELL_HPP
