#include "InputBelt.h"
#include "json.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <random>
#include <stdexcept>

BoxGeneratorParams BoxGeneratorParams::fromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("BoxGeneratorParams::fromFile: cannot open " + path);

    nlohmann::json j = nlohmann::json::parse(f);

    BoxGeneratorParams p;
    if (j.contains("num_boxes"))        p.num_boxes        = j["num_boxes"];
    if (j.contains("num_destinations")) p.num_destinations = j["num_destinations"];
    if (j.contains("seed"))             p.seed             = j["seed"].get<uint64_t>();
    if (j.contains("weights"))
        p.weights = j["weights"].get<std::unordered_map<Family, double>>();
    return p;
}

void InputBelt::push(Box b) {
    queue_.push_back(std::move(b));
}

std::optional<Box> InputBelt::pop() {
    if (queue_.empty()) return std::nullopt;
    Box b = std::move(queue_.front());
    queue_.pop_front();
    return b;
}

std::optional<Box> InputBelt::peek() const {
    if (queue_.empty()) return std::nullopt;
    return queue_.front();
}

bool        InputBelt::empty() const { return queue_.empty(); }
std::size_t InputBelt::size()  const { return queue_.size(); }

// ---------------------------------------------------------------------------

struct DestEntry {
    Family      name;
    std::string dest_code;  // 8-digit string
};

static const std::vector<DestEntry>& destTable() {
    static const std::vector<DestEntry> table = {
        {"zara_es",          "01100001"},
        {"zara_fr",          "01100002"},
        {"zara_de",          "01100003"},
        {"zara_uk",          "01100004"},
        {"zara_it",          "01100005"},
        {"zara_pt",          "01100006"},
        {"bershka_es",       "01200001"},
        {"bershka_fr",       "01200002"},
        {"bershka_de",       "01200003"},
        {"bershka_uk",       "01200004"},
        {"stradivarius_es",  "01300001"},
        {"stradivarius_fr",  "01300002"},
        {"stradivarius_de",  "01300003"},
        {"pull_bear_es",     "01400001"},
        {"pull_bear_fr",     "01400002"},
        {"pull_bear_de",     "01400003"},
        {"massimo_dutti_es", "01500001"},
        {"massimo_dutti_fr", "01500002"},
        {"oysho_es",         "01600001"},
        {"oysho_fr",         "01600002"},
        {"zara_home_es",     "01700001"},
        {"zara_home_fr",     "01700002"},
        {"lefties_es",       "01800001"},
    };
    return table;
}

const std::vector<Family>& InputBelt::availableDestinations() {
    static std::vector<Family> names;
    if (names.empty()) {
        for (const auto& e : destTable()) names.push_back(e.name);
    }
    return names;
}

// Formats bulk counter as zero-padded 5-digit string.
static std::string fmtBulk(int n) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%05d", n);
    return buf;
}

InputBelt InputBelt::generate(const BoxGeneratorParams& p) {
    const auto& table = destTable();
    int n_dest = std::max(1, std::min(p.num_destinations, static_cast<int>(table.size())));

    // Build probability vector (normalise weights or uniform).
    std::vector<double> probs(n_dest, 1.0);
    if (!p.weights.empty()) {
        for (int i = 0; i < n_dest; ++i) {
            auto it = p.weights.find(table[i].name);
            probs[i] = (it != p.weights.end()) ? it->second : 1.0;
        }
    }

    std::mt19937_64                      rng(p.seed);
    std::discrete_distribution<int>      dist(probs.begin(), probs.end());
    std::vector<int>                     batch_counter(n_dest, 0);

    InputBelt belt;
    for (int i = 0; i < p.num_boxes; ++i) {
        int         idx    = dist(rng);
        const auto& entry  = table[idx];
        int         bulk   = ++batch_counter[idx];
        // ID = source(7) + dest_code(8) + bulk(5) = 20 chars
        BoxId id = "3010028" + entry.dest_code + fmtBulk(bulk);
        belt.push(Box(id, entry.name, Tick{0}));
    }
    return belt;
}
