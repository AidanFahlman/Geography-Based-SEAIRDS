//
// Created by binybrion on 6/30/20.
// Modified by Glenn 02/07/20
// Modified by Aidan 04/04/21

#ifndef PANDEMIC_HOYA_2002_seaird_HPP
#define PANDEMIC_HOYA_2002_seaird_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include "hysteresis_factor.hpp"

struct seaird {
    std::vector<double> age_group_proportions;
    std::vector<double> susceptible;
    std::vector<std::vector<double>> exposed;
    std::vector<std::vector<double>> infected;
    std::vector<std::vector<double>> asymptomatic;
    std::vector<std::vector<double>> recovered;
    std::vector<double> fatalities;
    std::unordered_map<std::string, hysteresis_factor> hysteresis_factors;
    double population;

    std::vector<double> disobedient;
    double hospital_capacity;
    double fatality_modifier;

    // Required for the JSON library, as types used with it must be default-constructable.
    // The overloaded constructor results in a default constructor having to be manually written.
    seaird() = default;

    seaird(std::vector<double> sus, std::vector<double> exp, std::vector<double> inf, std::vector<double> asym,
          std::vector<double> rec, double fat, double dis, double hcap, double fatm, double asym_r) :
            susceptible{std::move(sus)}, exposed{std::move(exp)}, infected{std::move(inf)}, asymptomatic{std::move(asym)},
            recovered{std::move(rec)}, fatalities{fat}, disobedient{dis},
            hospital_capacity{hcap}, fatality_modifier{fatm} {}

    unsigned int get_num_age_segments() const {
        return susceptible.size();
    }

    unsigned int get_num_exposed_phases() const {
        return exposed.front().size();
    }

    unsigned int get_num_infected_phases() const {
        return infected.front().size();
    }

    unsigned int get_num_asymptomatic_phases() const {
        return asymptomatic.front().size();
    }

    unsigned int get_num_recovered_phases() const {
        return recovered.front().size();
    }

    static double sum_state_vector(const std::vector<double> &state_vector) {
        return std::accumulate(state_vector.begin(), state_vector.end(), 0.0f);
    }

    double get_total_fatalities() const {
        double total_fatalities = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_fatalities += fatalities.at(i) * age_group_proportions.at(i);
        }
        return total_fatalities;
    }

    double get_total_exposed() const {
        float total_exposed = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_exposed += sum_state_vector(exposed.at(i)) * age_group_proportions.at(i);
        }
        return total_exposed;
    }
    
    double get_total_infections() const {
        float total_infections = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_infections += sum_state_vector(infected.at(i)) * age_group_proportions.at(i);
        }
        return total_infections;
    }

    double get_total_asymptomatic() const {
        float total_asymptomatic = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_asymptomatic += sum_state_vector(asymptomatic.at(i)) * age_group_proportions.at(i);
        }
        return total_asymptomatic;
    }

    double get_total_recovered() const {
        double total_recoveries = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_recoveries += sum_state_vector(recovered.at(i)) * age_group_proportions.at(i);
        }
        return total_recoveries;
    }

    double get_total_susceptible() const {
        double total_susceptible = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_susceptible += susceptible.at(i) * age_group_proportions.at(i);
        }
        return total_susceptible;
    }

    bool operator!=(const seaird &other) const {
        return (susceptible != other.susceptible) || (exposed != other.exposed) || (infected != other.infected) || (asymptomatic != other.asymptomatic)|| (recovered != other.recovered);
    }
};

bool operator<(const seaird &lhs, const seaird &rhs) { return true; }


// outputs <population, S, E, I, R, new I, new E, new R, D>
std::ostream &operator<<(std::ostream &os, const seaird &seaird) {

    double new_exposed = 0.0f;
    double new_infections = 0.0f;
    double new_asymptomatic = 0.0f;
    double new_recoveries = 0.0f;

    for(int i = 0; i < seaird.age_group_proportions.size(); ++i) {
        new_exposed += seaird.exposed.at(i).at(0) * seaird.age_group_proportions.at(i);
        new_infections += seaird.infected.at(i).at(0) * seaird.age_group_proportions.at(i);
        new_asymptomatic += seaird.asymptomatic.at(i).at(0) * seaird.age_group_proportions.at(i);
        new_recoveries += seaird.recovered.at(i).at(0) * seaird.age_group_proportions.at(i);
    }

    os << "<" << seaird.population - seaird.population * seaird.get_total_fatalities() << "," << seaird.get_total_susceptible()
        << "," << seaird.get_total_exposed() << "," << seaird.get_total_infections() << "," << seaird.get_total_recovered() << ","
        << new_exposed << "," << new_infections << "," << new_recoveries << "," << seaird.get_total_fatalities() <<
        "," << new_asymptomatic << "," << seaird.get_total_asymptomatic() <<">";
    return os;
}

void from_json(const nlohmann::json &json, seaird &current_seaird) {
    json.at("age_group_proportions").get_to(current_seaird.age_group_proportions);
    json.at("infected").get_to(current_seaird.infected);
    json.at("asymptomatic").get_to(current_seaird.asymptomatic);
    json.at("recovered").get_to(current_seaird.recovered);
    json.at("susceptible").get_to(current_seaird.susceptible);
    json.at("exposed").get_to(current_seaird.exposed);
    json.at("fatalities").get_to(current_seaird.fatalities);
    json.at("disobedient").get_to(current_seaird.disobedient);
    json.at("hospital_capacity").get_to(current_seaird.hospital_capacity);
    json.at("fatality_modifier").get_to(current_seaird.fatality_modifier);
    json.at("population").get_to(current_seaird.population);
    
    assert(current_seaird.age_group_proportions.size() == current_seaird.susceptible.size() && current_seaird.age_group_proportions.size() == current_seaird.exposed.size()
    	&& current_seaird.age_group_proportions.size() == current_seaird.infected.size() && current_seaird.age_group_proportions.size() == current_seaird.recovered.size()
    	&& "There must be an equal number of age groups between age_group_proportions, susceptible, exposed, infected, and recovered!\n");
}

#endif //PANDEMIC_HOYA_2002_seaird_HPP
