#pragma once
#include "simulation.h"
#include "Box.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Parse silo-semi-empty.csv and return one PrePlacedBox per stored box.
//
// CSV columns:  posicion (11 chars), etiqueta (box label)
// posicion layout:
//   [0:2]  aisle  1-based → 0-based aisle_idx
//   [2:4]  side   1 or 2
//   [4:7]  slot x 1-based → 0-based x
//   [7:9]  y level
//   [9:11] z depth (1 or 2)
// family = etiqueta[7:15]
inline std::vector<PrePlacedBox> loadSiloCSV(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("loadSiloCSV: cannot open '" + path + "'");

    std::vector<PrePlacedBox> result;
    std::string line;
    std::getline(f, line); // skip header

    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string posicion, etiqueta;
        if (!std::getline(ss, posicion, ',')) continue;
        if (!std::getline(ss, etiqueta,  ',')) continue;

        // trim carriage returns (Windows line endings)
        while (!etiqueta.empty() && (etiqueta.back() == '\r' || etiqueta.back() == '\n'))
            etiqueta.pop_back();
        while (!posicion.empty() && (posicion.back() == '\r' || posicion.back() == '\n'))
            posicion.pop_back();

        if (posicion.size() < 11 || etiqueta.size() < 15) continue;

        int aisle_idx = std::stoi(posicion.substr(0, 2)) - 1;
        int side      = std::stoi(posicion.substr(2, 2));
        int x         = std::stoi(posicion.substr(4, 3)) - 1;
        int y         = std::stoi(posicion.substr(7, 2));
        int z         = std::stoi(posicion.substr(9, 2));

        std::string family = etiqueta.substr(7, 8);

        Position pos;
        pos.x    = x;
        pos.y    = y;
        pos.z    = z;
        pos.side = side;

        result.emplace_back(Box(etiqueta, family, 0), aisle_idx, pos);
    }

    return result;
}
